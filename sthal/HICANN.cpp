#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/serialization/weak_ptr.hpp>

#include "sthal/HICANN.h"
#include "sthal/FPGA.h"
#include "sthal/Wafer.h"
#include "sthal/Settings.h"
#include "sthal_exception.h"

#include "hal/Coordinate/iter_all.h"

#include <stdexcept>

#include "pythonic/zip.h"

#include "bitter/util.h"

#include <bitset>
#include <sstream>
#include <boost/algorithm/string/join.hpp>
#include <iomanip>

#include <log4cxx/logger.h>

using namespace ::HMF::Coordinate;

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("sthal.HICANN");

namespace sthal {

HICANN::HICANN() : mWafer(nullptr)
{
	// Enable membrane reset and firing
	enable_firing();
	// Disable l1 out to avoid spam from used neurons
	disable_l1_output();

	for (auto merger : iter_all<DNCMergerOnHICANN>())
	{
		layer1[merger].slow = true;
	}

	// TODO check default
	use_big_capacitors(true);
	set_fg_speed_up_scaling(NORMAL);
}

HICANN::HICANN(const hicann_coord& hicann)
    : HICANN()
{
	mCoordinate = hicann;
}

HICANN::HICANN(const hicann_coord& hicann, boost::shared_ptr<FPGA> const fpga)
    : HICANN(hicann)
{
	mFPGA = fpga;
}

HICANN::HICANN(const hicann_coord& hicann, boost::shared_ptr<FPGA> const fpga, Wafer* wafer)
    : HICANN(hicann, fpga)
{
	mWafer = wafer;
}

HICANN::~HICANN()
{
}

HICANN::hicann_coord const& HICANN::index() const
{
	return mCoordinate;
}

bool operator==(const HICANN & a, const HICANN & b)
{
	// check sequence-of-optionals manually (due to bool-context)
	for (auto [elem_a, elem_b] : pythonic::zip(a.mADCConfig, b.mADCConfig)) {
		if (static_cast<bool>(elem_a) != static_cast<bool>(elem_b)) {
			return false;
		} else if (static_cast<bool>(elem_a) && (! ((*elem_a) == (*elem_b)))) {
			return false;
		}
	}

	return (static_cast<const HICANNData &>(a) == static_cast<const HICANNData &>(b))
		&& (a.mCoordinate == b.mCoordinate)
		// no recursion! TODO: remove cycles in data structures
		// && ((a.mFPGA.expired() == b.mFPGA.expired()) && (!a.mFPGA.expired()) && (*(a.mFPGA.lock()) == *(b.mFPGA.lock()))) 
		// && ((a.mWafer == b.mWafer) && (a.mWafer && ((*a.mWafer) == (*b.mWafer)))
		&& (a.mVersion == b.mVersion)
		&& (a.mMinorVersion == b.mMinorVersion)
	;
}

bool operator!=(const HICANN & a, const HICANN & b)
{
	return !(a == b);
}

ADCConfig HICANN::getADCConfig(const ::HMF::Coordinate::AnalogOnHICANN & ii)
{
	if(!mADCConfig[ii].has_value()) {
		wafer().populate_adc_config(index().toHICANNOnWafer(), ii);
	}

	if (mADCConfig[ii].has_value()) {
		return *(mADCConfig[ii].value());
	} else {
		std::stringstream err;
		err << "No ADC found for " << ii << " on " << index();
		throw not_found(err.str());
	}
}

void HICANN::setADCConfig(const ::HMF::Coordinate::AnalogOnHICANN & ii,
		const ADCConfig & adc)
{
	mADCConfig[ii].emplace(boost::make_shared<ADCConfig>(adc));
}

/// Clear adc config
void HICANN::resetADCConfig()
{
    for (auto & cfg : mADCConfig)
    {
        cfg.reset();
    }
	for (auto analog : iter_all<AnalogOnHICANN>() ) {
		mADCConfig[analog].reset();
	}
}

void HICANN::set_version(size_t version) {
	mVersion = version;
}

size_t HICANN::get_version() const
{
	if (mVersion > 0) {
		return mVersion;
	} else {
		throw not_found("Version is only available after connecting to hardware");
	}
}

AnalogRecorder
HICANN::analogRecorder(const ::HMF::Coordinate::AnalogOnHICANN & ii)
{
	if(!mADCConfig[ii].has_value()) {
		wafer().populate_adc_config(index().toHICANNOnWafer(), ii);
	}

	if (mADCConfig[ii].has_value())
	{
		if (!analog.enabled(ii))
		{
			LOG4CXX_WARN(logger, "You are trying to get an AnalogRecorder on "
					     << index() << " for " << ii << ", but this analog "
						 "output is not enabled");
		}
		return AnalogRecorder(*(mADCConfig[ii].value()));
	}
	else
		throw std::runtime_error("Could not get AnalogRecorder. AnalogRecorder can be only used after"
								 " connecting to the hardware. If already connected, check for missing"
								 " ADC config in database.");
}

void HICANN::disable_aout()
{
	for (auto neuron : iter_all<neuron_coord>())
	{
		neurons[neuron].enable_aout(false);
	}
}


void HICANN::enable_aout(const neuron_coord & ii, const analog_coord & analog_c)
{
	FGBlockOnHICANN fg_block = ii.toSharedFGBlockOnHICANN();

	for (auto neuron : iter_all<neuron_coord>())
	{
		if (neuron.toSharedFGBlockOnHICANN() == fg_block)
		{
			neurons[neuron].enable_aout(false);
		}
	}

	neurons[ii].enable_aout(true);

	switch (fg_block.id())
	{
		case 0:
			analog.set_membrane_top_even(analog_c);
			break;
		case 1:
			analog.set_membrane_top_odd(analog_c);
			break;
		case 2:
			analog.set_membrane_bot_even(analog_c);
			break;
		case 3:
			analog.set_membrane_bot_odd(analog_c);
			break;
		default:
			assert(false);
	}
}

void HICANN::enable_l1_output(const neuron_coord & ii, const L1Address & addr)
{
	auto & nrn = neurons[ii];
	nrn.address(addr);
	nrn.enable_spl1_output(true);
}

void HICANN::disable_l1_output()
{
	for (auto neuron : iter_all<neuron_coord>())
	{
		neurons[neuron].enable_spl1_output(false);
	}
}

void HICANN::disable_l1_output(const neuron_coord & ii)
{
	auto & nrn = neurons[ii];
	nrn.enable_spl1_output(false);
}

void HICANN::enable_firing()
{
	for (auto neuron : iter_all<neuron_coord>())
	{
		neurons[neuron].activate_firing(true);
	}
}

void HICANN::enable_firing(const neuron_coord & ii)
{
	auto & nrn = neurons[ii];
	nrn.activate_firing(true);
}

void HICANN::disable_firing()
{
	for (auto neuron : iter_all<neuron_coord>())
	{
		neurons[neuron].activate_firing(false);
	}
}

void HICANN::sendSpikes(const dnc_merger_coord & link, const SpikeVector & s)
{
	fpga()->addSendSpikes(mCoordinate, link, s);
}

void HICANN::sortSpikes()
{
	fpga()->sortSpikes();
}

SpikeVector HICANN::receivedSpikes(const dnc_merger_coord & link) const
{
	return fpga()->getReceivedSpikes(mCoordinate, link);
}

SpikeVector HICANN::sentSpikes(const dnc_merger_coord & link) const
{
	return fpga()->getSentSpikes(mCoordinate, link);
}

void HICANN::setCurrentStimulus(const neuron_coord & ii, const FGStimulus & stim)
{
	FGBlockOnHICANN fg_block = ii.toSharedFGBlockOnHICANN();
	enable_current_stimulus(ii);
	current_stimuli[fg_block.id()] = stim;
}

void HICANN::disable_current_stimulus()
{
	for (auto neuron : iter_all<neuron_coord>())
	{
		neurons[neuron].enable_current_input(false);
	}
}

void HICANN::enable_current_stimulus(const neuron_coord & ii)
{
	FGBlockOnHICANN fg_block = ii.toSharedFGBlockOnHICANN();

	for (auto neuron : iter_all<neuron_coord>())
	{
		if (neuron.toSharedFGBlockOnHICANN() == fg_block)
		{
			neurons[neuron].enable_current_input(false);
		}
	}

	neurons[ii].enable_current_input(true);
}


FGStimulus & HICANN::getCurrentStimulus(const neuron_coord & ii)
{
	FGBlockOnHICANN fg_block = ii.toSharedFGBlockOnHICANN();
	return current_stimuli[fg_block.id()];
}

std::vector<double> HICANN::getStimCurrents(const neuron_coord & ii)
{
	FGStimulus stim = HICANN::getCurrentStimulus(ii);
	std::vector<double> currents(HMF::HICANN::FGBlock::fg_columns);
	for (size_t i=0; i<HMF::HICANN::FGBlock::fg_columns; i++)
	{
		// returned currents are in uA
		currents[i] = (double)stim[i] * 2.5 / 1023;
	}
	return currents;
}

std::vector<double> HICANN::getStimTimestamps(const neuron_coord & ii)
{
	FGStimulus stim = HICANN::getCurrentStimulus(ii);
	const uint8_t pulselength = stim.getPulselength();
	std::vector<double> times(HMF::HICANN::FGBlock::fg_columns);
	const double step = 4.0 / fpga()->commonFPGASettings()->getPLL();
	for (size_t i=0; i<HMF::HICANN::FGBlock::fg_columns; i++)
	{
		times[i] = i * step * (pulselength+1);
	}
	return times;
}

void HICANN::use_big_capacitors(bool enable)
{
	neurons.config.bigcap[top] = neurons.config.bigcap[bottom] = enable;
}

bool HICANN::get_bigcap_setting()
{
	assert(neurons.config.bigcap[top] == neurons.config.bigcap[bottom]);
	return neurons.config.bigcap[top];
}

void HICANN::set_fg_speed_up_scaling(SpeedUp speedup)
{
	set_speed_up_gl(speedup);
	set_speed_up_gladapt(speedup);
	set_speed_up_radapt(speedup);
}

void HICANN::set_speed_up_gl(SpeedUp speedup)
{
	if (speedup != NORMAL && speedup != FAST && speedup != SLOW)
		throw std::invalid_argument("Invalid speedup factor");

	bool slow = (speedup == SLOW);
	bool fast = (speedup == FAST);
	neurons.config.slow_I_gl[top] = neurons.config.slow_I_gl[bottom] = slow;
	neurons.config.fast_I_gl[top] = neurons.config.fast_I_gl[bottom] = fast;
}

void HICANN::set_speed_up_gladapt(SpeedUp speedup)
{
	if (speedup != NORMAL && speedup != FAST && speedup != SLOW)
		throw std::invalid_argument("Invalid speedup factor");

	bool slow = (speedup == SLOW);
	bool fast = (speedup == FAST);
	neurons.config.slow_I_gladapt[top] = neurons.config.slow_I_gladapt[bottom] = slow;
	neurons.config.fast_I_gladapt[top] = neurons.config.fast_I_gladapt[bottom] = fast;
}

void HICANN::set_speed_up_radapt(SpeedUp speedup)
{
	if (speedup != NORMAL && speedup != FAST && speedup != SLOW)
		throw std::invalid_argument("Invalid speedup factor");

	bool slow = (speedup == SLOW);
	bool fast = (speedup == FAST);
	neurons.config.slow_I_radapt[top] = neurons.config.slow_I_radapt[bottom] = slow;
	neurons.config.fast_I_radapt[top] = neurons.config.fast_I_radapt[bottom] = fast;
}

HICANN::SpeedUp HICANN::get_speed_up_gl()
{
	if (neurons.config.slow_I_gl[top] != neurons.config.slow_I_gl[bottom])
		throw std::logic_error("Speedup bit gl slow: top and bottom are not equal");
	if (neurons.config.fast_I_gl[top] != neurons.config.fast_I_gl[bottom])
		throw std::logic_error("Speedup bit gl fast: top and bottom are not equal");

	bool slow = neurons.config.slow_I_gl[top];
	bool fast = neurons.config.fast_I_gl[top];
	if (slow && fast)
		throw std::logic_error("Speedup bit gl is slow and fast");

	if (slow)
		return SLOW;
	else if (fast)
		return FAST;
	else
		return NORMAL;
}

HICANN::SpeedUp HICANN::get_speed_up_gladapt()
{
	if (neurons.config.slow_I_gladapt[top] != neurons.config.slow_I_gladapt[bottom])
		throw std::logic_error("Speedup bit gladapt slow: top and bottom are not equal");
	if (neurons.config.fast_I_gladapt[top] != neurons.config.fast_I_gladapt[bottom])
		throw std::logic_error("Speedup bit gladapt fast: top and bottom are not equal");

	bool slow = neurons.config.slow_I_gladapt[top];
	bool fast = neurons.config.fast_I_gladapt[top];
	if (slow && fast)
		throw std::logic_error("Speedup bit gladapt is slow and fast");

	if (slow)
		return SLOW;
	else if (fast)
		return FAST;
	else
		return NORMAL;
}

HICANN::SpeedUp HICANN::get_speed_up_radapt()
{
	if (neurons.config.slow_I_radapt[top] != neurons.config.slow_I_radapt[bottom])
		throw std::logic_error("Speedup bit radapt slow: top and bottom are not equal");
	if (neurons.config.fast_I_radapt[top] != neurons.config.fast_I_radapt[bottom])
		throw std::logic_error("Speedup bit radapt fast: top and bottom are not equal");

	bool slow = neurons.config.slow_I_radapt[top];
	bool fast = neurons.config.fast_I_radapt[top];
	if (slow && fast)
		throw std::logic_error("Speedup bit radapt is slow and fast");

	if (slow)
		return SLOW;
	else if (fast)
		return FAST;
	else
		return NORMAL;
}

size_t HICANN::capacity()
{
	return ::HMF::Coordinate::NeuronOnHICANN::enum_type::end;
}

void HICANN::clear()
{
	static_cast<HICANNData*>(this)->operator=(HICANNData());
}

void HICANN::clear_l1_switches()
{
	synapse_switches.clear();
	crossbar_switches.clear();
}

void HICANN::clear_l1_routing()
{
	clear_l1_switches();
	synapses.clear_drivers();
}

void HICANN::clear_complete_l1_routing()
{
	clear_l1_switches();
	synapses.clear_drivers();
	repeater.clearReapeater();
}

void HICANN::clear_synapse_controllers()
{
	synapse_controllers = SynapseControllers();
}

std::vector<NeuronOnHICANN> HICANN::set_neuron_size(const size_t n)
{
	std::vector<NeuronOnHICANN> ret;

	if (n != 1 && n != 2 && n != 4 && n != 8 && n != 16 && n != 32 && n != 64)
	{
		throw std::runtime_error("Neuron size must be 1, 2, 4, 8, 16, 32 or 64");
	}

	if (n == 1)
	{
		for (auto quad_c : iter_all< ::HMF::Coordinate::QuadOnHICANN>())
		{
			auto & quad = neurons[quad_c];
			quad.setVerticalInterconnect(X(0), false);
			quad.setVerticalInterconnect(X(1), false);
			quad.setHorizontalInterconnect(Y(0), false);
			quad.setHorizontalInterconnect(Y(1), false);
		}

		for (auto neuron : iter_all<neuron_coord>())
		{
			neurons[neuron].enable_fire_input(false);
			neurons[neuron].activate_firing(true);
			ret.push_back(neuron);
		}
		return ret;
	}

	const size_t end = NeuronOnHICANN::x_type::end;
	for (size_t begin = 0; begin < end; begin += n/2)
	{
		X x{begin};
		connect_denmems(x, X(x + n/2 - 1));
		ret.push_back(NeuronOnHICANN{x, Y{0}});
	}
	return ret;
}

namespace {
	using namespace ::HMF::Coordinate;
	// Connects to the Neuron
	void connect_horizontally(Neurons & neurons, X left, bool connect)
	{
		if (left.value() >= 255)
			return;

		X right{left + 1};
		NeuronOnHICANN lt{left, Y(0)};
		NeuronOnHICANN rt{right, Y(0)};
		NeuronOnHICANN lb{left, Y(1)};
		NeuronOnHICANN rb{right, Y(1)};

		if (lt.toQuadOnHICANN() == rt.toQuadOnHICANN())
		{
			neurons[lt.toQuadOnHICANN()].setHorizontalInterconnect(Y(0), connect);
			neurons[lt.toQuadOnHICANN()].setHorizontalInterconnect(Y(1), connect);
		}
		else
		{
			neurons[lt].enable_fire_input(false);
			neurons[rt].enable_fire_input(connect);
			neurons[lb].enable_fire_input(false);
			neurons[rb].enable_fire_input(false);
		}
	}
	// sets a horizontal connection from left neuron to its right neighbour neuron with specified direction
	void connect_horizontally(Neurons & neurons, NeuronOnHICANN left_neuron, bool connect, bool to_right)
	{
		if (left_neuron.x() >= 255)
			return;
		NeuronOnHICANN right_neuron(X(left_neuron.x()+1), left_neuron.y());

		if (left_neuron.toQuadOnHICANN() == right_neuron.toQuadOnHICANN())
		{
			// connection within Quad is not directed
			neurons[left_neuron.toQuadOnHICANN()].setHorizontalInterconnect(left_neuron.y(), connect);
		}
		else
		{
			// connection between Quad is directed
			neurons[left_neuron].enable_fire_input(connect & (!to_right));
			neurons[right_neuron].enable_fire_input(connect & to_right);
		}
	}
}

// We connect a block of denmems and let the upper left fire
NeuronOnHICANN HICANN::connect_denmems(X first_column, X last_column)
{
	NeuronOnHICANN first{first_column, Y{0}};
	if (first_column == X(0))
		connect_horizontally(neurons, X{first_column-1}, false);

	for (size_t ii = first_column; ii <= last_column; ++ii)
	{
		NeuronOnHICANN nt{X{ii}, Y{0}};
		NeuronOnHICANN nb{X{ii}, Y{1}};
		neurons[nt].activate_firing(false);
		neurons[nb].activate_firing(false);
		neurons[nt.toQuadOnHICANN()].setVerticalInterconnect(nt.toNeuronOnQuad().x(), true);
		connect_horizontally(neurons, X{ii}, ii != last_column);
	}
	neurons[first].activate_firing(true);
	return first;
}

void HICANN::connect_denmems(X first_column, X last_column, NeuronOnHICANN firing_denmem)
{
	if (first_column > firing_denmem.x() ||  firing_denmem.x() > last_column )
		throw std::runtime_error("HICANN::connect_denmems(..): firing_denmem must be within first_column and last_column");

	X firing_column(firing_denmem.x());
	Y firing_row(firing_denmem.y());
	Y other_row((firing_row.value()+1)%2);

	for (size_t ii = first_column; ii <= last_column; ++ii)
	{
		NeuronOnHICANN nt{X{ii}, Y{0}};
		NeuronOnHICANN nb{X{ii}, Y{1}};
		neurons[nt].activate_firing(false);
		neurons[nb].activate_firing(false);
		neurons[nt.toQuadOnHICANN()].setVerticalInterconnect(nt.toNeuronOnQuad().x(), true);
		connect_horizontally(neurons, NeuronOnHICANN(X(ii),firing_row), /*connect*/ ii != last_column, /*to_right*/ ii >= firing_column);
		connect_horizontally(neurons, NeuronOnHICANN(X(ii),other_row), /*connect*/ false, /*to_right*/ ii >= firing_column);
	}

	neurons[firing_denmem].activate_firing(true);
}

bool HICANN::check(std::ostream & errors) const
{

	auto const& settings = Settings::get();

	bool ok = true;
	ok &= neurons.check(errors);
	ok &= repeater.check(errors);
	ok &= crossbar_switches.check(settings.crossbar_switches.max_switches_per_row,
	                              settings.crossbar_switches.max_switches_per_column,
	                              errors);
	ok &= synapse_switches.check(
	    settings.synapse_switches.max_switches_per_row,
	    settings.synapse_switches.max_switches_per_column_per_side, errors);
	return ok;
}

std::string HICANN::check() const
{
	std::stringstream err;
	if (check(err))
		return std::string();
	else
		return err.str();
}

boost::shared_ptr<FPGA> HICANN::fpga()
{
	if(boost::shared_ptr<FPGA> fpga = mFPGA.lock())
	{
		return fpga;
	}
	throw std::runtime_error("Incomplete initialized HICANN has no FPGA");
}

boost::shared_ptr<const FPGA> HICANN::fpga() const
{
	if(boost::shared_ptr<FPGA> fpga = mFPGA.lock())
	{
		return fpga;
	}
	throw std::runtime_error("Incomplete initialized HICANN has no FPGA");
}

bool HICANN::has_wafer() const {
	return static_cast<bool>(mWafer);
}

Wafer& HICANN::wafer()
{
	if (mWafer) {
		return *mWafer;
	}
	throw std::runtime_error("Incomplete initialized HICANN has no Wafer");
}

const Wafer& HICANN::wafer() const
{
	if (mWafer) {
		return *mWafer;
	}
	throw std::runtime_error("Incomplete initialized HICANN has no Wafer");
}

std::ostream& operator<<(std::ostream& os, const sthal::HICANN & h)
{

	// FIXME: print out more information. at the moment, it's only synapse setting

	for (auto s_coord : iter_all<sthal::HICANN::synapse_driver_coord>()) {

		auto& sd = h.synapses[s_coord];

		if (!sd.is_enabled()) {
			continue;
		}

		os << s_coord << '\n' << sd << '\n';

		const sthal::HICANN::synapse_row_coord c_top_row(s_coord, geometry::top);
		const sthal::HICANN::synapse_row_coord c_bottom_row(s_coord, geometry::bottom);

		const auto& top_row_proxy    = h.synapses[c_top_row];
		const auto& bottom_row_proxy = h.synapses[c_bottom_row];

		// even neuron id -> top strobe line
		// odd neuron id -> bottom strobe line
		const auto& top_even_driver_decoder    = sd[geometry::top].get_decoder(geometry::top);
		const auto& bottom_even_driver_decoder = sd[geometry::bottom].get_decoder(geometry::top);
		const auto& top_odd_driver_decoder     = sd[geometry::top].get_decoder(geometry::bottom);
		const auto& bottom_odd_driver_decoder  = sd[geometry::bottom].get_decoder(geometry::bottom);

		std::vector<std::string> top;    top.reserve(256);
		std::vector<std::string> bottom; bottom.reserve(256);

		bool even = true;

		for (auto item : pythonic::zip(top_row_proxy.decoders,
									   bottom_row_proxy.decoders)) {

			auto& top_row_synapse_decoder  = std::get<0>(item);
			auto& bottom_row_synapse_decoder = std::get<1>(item);

			const auto& top_driver_decoder    = even ? top_even_driver_decoder : top_odd_driver_decoder;
			const auto& bottom_driver_decoder = even ? bottom_even_driver_decoder : bottom_odd_driver_decoder;

			const auto bit_top_row    = bit::concat(std::bitset<2>(top_driver_decoder.value()),    std::bitset<4>(top_row_synapse_decoder.value()));
			const auto bit_bottom_row = bit::concat(std::bitset<2>(bottom_driver_decoder.value()), std::bitset<4>(bottom_row_synapse_decoder.value()));

			const auto top_row_address = bit_top_row.to_ulong();
			const auto bottom_row_address = bit_bottom_row.to_ulong();

			std::stringstream ss;

			ss << std::setw(2) << std::setfill('0') << top_row_address;
			top.emplace_back(ss.str());

			ss.str("");
			ss.clear();
			ss << std::setw(2) << std::setfill('0') << bottom_row_address;
			bottom.emplace_back(ss.str());

			even = !even; // alternate

		}

		os << "Decoded addresses: " << '\n';

		// delimeter without spaces to avoid that terminal does linebreaks
		os << boost::algorithm::join(top, ",") << '\n';
		os << boost::algorithm::join(bottom, ",") << '\n';

		os << '\n';

		std::vector<std::string> top_weights;    top_weights.reserve(256);
		std::vector<std::string> bottom_weights; bottom_weights.reserve(256);

		for (auto item : pythonic::zip(top_row_proxy.weights,
									   bottom_row_proxy.weights)) {

			auto& top_row_weight  = std::get<0>(item);
			auto& bottom_row_weight = std::get<1>(item);

			std::stringstream ss;

			ss << std::setw(2) << std::setfill('0') << int(top_row_weight.value());
			top_weights.emplace_back(ss.str());

			ss.str("");
			ss.clear();
			ss << std::setw(2) << std::setfill('0') << int(bottom_row_weight.value());
			bottom_weights.emplace_back(ss.str());

		}

		os << "Weights: " << '\n';

		// delimeter without spaces to avoid that terminal does linebreaks
		os << boost::algorithm::join(top_weights, ",") << '\n';
		os << boost::algorithm::join(bottom_weights, ",") << '\n';

		os << '\n';

	}

	return os;

}

void HICANN::xml_dump(const char * const _filename, bool overwrite) const
{
	boost::filesystem::path filename(_filename);
	if (!overwrite && boost::filesystem::exists(filename))
	{
		std::string err("File '");
		err += filename.c_str();
		err += "' allready exists!";
		throw std::runtime_error(err);
	}
	boost::filesystem::ofstream stream(filename);
	boost::archive::xml_oarchive oa(stream);
	oa << boost::serialization::make_nvp("hicann", *this);
}

void HICANN::route(DNCMergerOnHICANN from,
		           SynapseDriverOnHICANN to,
				   size_t num)
{
	if (num > 3)
		throw std::invalid_argument("non-existent route");

	HRepeaterOnHICANN const repeater_coord =
		from.toSendingRepeaterOnHICANN().toHRepeaterOnHICANN();
	HLineOnHICANN const out_line = repeater_coord.toHLineOnHICANN();
	VLineOnHICANN const v_line = out_line.toVLineOnHICANN(to.toSideHorizontal())[num];
	SynapseDriverOnHICANN::y_type driver_line = to.line();

	LOG4CXX_DEBUG(logger,
		"Connecting " << from << " -> " << repeater_coord << " -> " <<
		out_line << " -> " << v_line << " -> " <<  driver_line << ") to " <<
		to);

	/// The synapses switches are the restraining factor, if it is not possible
	/// to route, it should fail here
	synapse_switches.set(v_line, driver_line, true);
	crossbar_switches.set(v_line, out_line, true);
	repeater[repeater_coord].setOutput(right, true);
}

::HMF::Coordinate::NeuronOnHICANN HICANN::find_neuron_in_analog_output(
	analog_coord analog_c) const
{
	// We will only check the neurons connected to an analog output.
	// The analog outputs have the same assignment as the shared floating gate
	// blocks, we use this to filter the neurons.
	FGBlockOnHICANN block;
	if (analog.get_membrane_top_even(analog_c)) {
		block = FGBlockOnHICANN(X(0), Y(0));
	} else if (analog.get_membrane_top_odd(analog_c)) {
		block = FGBlockOnHICANN(X(1), Y(0));
	} else if (analog.get_membrane_bot_even(analog_c)) {
		block = FGBlockOnHICANN(X(0), Y(1));
	} else if (analog.get_membrane_bot_odd(analog_c)) {
		block = FGBlockOnHICANN(X(1), Y(1));
	} else {
		throw not_found(
			"Analog output is not connected to any neurons");
	}

	size_t neurons_found = 0;
	::HMF::Coordinate::NeuronOnHICANN last;
	for (auto neuron : iter_all<NeuronOnHICANN>()) {
		if (neuron.toSharedFGBlockOnHICANN() == block
			    && neurons[neuron].enable_aout()) {
			++neurons_found;
			last = neuron;
	    }
	}
	if (neurons_found == 0) {
		throw not_found(
			"No neuron connected to this analog output has its analog output "
		    "enabled");
	} else if (neurons_found > 1) {
		/// Bad configuration this is an real error
		throw std::runtime_error(
			"Multiple neurons connected to this analog output have their "
			"analog output enabled");
	} else {
		return last;
	}
}

template<typename Archiver>
void HICANN::serialize(Archiver& ar, unsigned int const)
{
	using namespace boost::serialization;
	ar & make_nvp("base", base_object<HICANNData>(*this))
	   & make_nvp("coordinate", mCoordinate)
	   & make_nvp("mFPGA", mFPGA)
	   & make_nvp("mWafer", mWafer)
	   //& make_nvp("mADCConfig", mADCConfig)
	   & make_nvp("mVersion", mVersion)
	   & make_nvp("mMinorVersion", mMinorVersion);
}


} // end namespace sthal

BOOST_CLASS_EXPORT_IMPLEMENT(sthal::HICANN)

#include "boost/serialization/serialization_helper.tcc"
EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(sthal::HICANN)
