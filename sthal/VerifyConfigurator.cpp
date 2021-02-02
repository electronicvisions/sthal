#include "sthal/VerifyConfigurator.h"

#include <algorithm>

#include <boost/algorithm/string/join.hpp>
#include <boost/range/iterator_range.hpp>

#include "halco/common/iter_all.h"
#include "halco/hicann/v2/format_helper.h"
#include "hal/HICANN/FGBlock.h"
#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"
#include "hal/backend/DNCBackend.h"
#include "hal/backend/FPGABackend.h"
#include "hal/backend/HICANNBackend.h"

#include "halco/common/typed_array.h"

#include "sthal/AnalogRecorder.h"
#include "sthal/FPGA.h"
#include "sthal/HICANN.h"
#include "sthal/SynapseControllerData.h"
#include "sthal/Timer.h"
#include "sthal/Wafer.h"

#include <log4cxx/logger.h>

using namespace ::halco::hicann::v2;
using namespace ::halco::common;

namespace sthal {

namespace {
const std::string INDENT("  ");

template <typename T>
std::string check(const T& expected, const T& read)
{
	if (!(expected == read)) {
		std::stringstream msg;
		msg << INDENT << INDENT << "configured: " << read << "\n";
		msg << INDENT << INDENT << "expected:   " << expected << "\n";
		return msg.str();
	} else {
		return std::string();
	}
}

template <typename Prefix, typename T>
std::string check(const Prefix pre, const T& expected, const T& read)
{
	if (!(expected == read)) {
		std::stringstream msg;
		msg << INDENT << pre << ":\n";
		msg << INDENT << INDENT << "configured: " << read << "\n";
		msg << INDENT << INDENT << "expected:   " << expected << "\n";
		return msg.str();
	} else {
		return std::string();
	}
}

template <typename Prefix, typename T>
std::string check(
    const Prefix pre,
    const T& expected,
    const T& read,
    const std::vector<SynapseOnWafer>& synapse_mask,
    SynapseOnWafer synapse)
{
	if (!(expected == read)) {
		if (std::find(synapse_mask.begin(), synapse_mask.end(), synapse) != synapse_mask.end()) {
			std::stringstream msg;
			msg << INDENT << pre << ":\n";
			msg << INDENT << INDENT << "configured: " << read << "\n";
			msg << INDENT << INDENT << "expected:   " << expected << "\n";
			return msg.str();
		}
	}
	return std::string();
}

template <typename XType, typename YType>
struct XYHelper
{
	XType x;
	YType y;

	friend std::ostream& operator<<(std::ostream& out, XYHelper<XType, YType> xy)
	{
		return out << xy.x << ", " << xy.y;
	}
};

template <typename XType, typename YType>
XYHelper<XType, YType> make_xy(XType x, YType y)
{
	return XYHelper<XType, YType>{x, y};
}

// Filters for components that can no longer be used on HICANN rev4
// Note: HICANN answers reads on these with bogous data
inline bool not_usable(const HICANN& h, const SynapseDriverOnHICANN c)
{
	const SynapseDriverOnHICANN first_invalid_row(Enum(110));
	const SynapseDriverOnHICANN last_invalid_row(Enum(113));
	return h.get_version() == 4 && c.toEnum() >= first_invalid_row.toEnum() &&
		c.toEnum() <= last_invalid_row.toEnum();
}

inline bool not_usable(const HICANN& h, const SynapseOnHICANN c)
{
	return not_usable(h, c.toSynapseDriverOnHICANN());
}

} // end namespace

std::ostream& operator<<(std::ostream& out, const VerificationResult& r) {
	if (r.readable) {
		out << short_format(r.hicann) << " " << r.subsystem << ": " << r.errors
		    << " errors in " << r.tested << " values";
		if (!r.reliable) {
			out << " (values are not reliable readable)";
		}
		if (!r.msg.empty()) {
			out << "\n" << r.msg;
		}
	} else {
		out << "not readable";
	}
	return out;
}

void VerifyConfigurator::data_not_readable(HICANNGlobal hicann, std::string subsystem)
{
	LOG4CXX_INFO(getLogger(), short_format(hicann) << " " << subsystem << ": "
	                                               << "NOT TESTED");
	mErrors.push_back(VerificationResult{hicann, subsystem, "", 0, 0, false, false});
}

void VerifyConfigurator::post_merge_errors(
    HICANNGlobal hicann, std::string subsystem, std::vector<std::string>& errors, bool reliable)
{
	auto valid_errors = boost::make_iterator_range(
	    errors.begin(),
	    std::remove_if(errors.begin(), errors.end(), std::mem_fn(&std::string::empty)));
	mErrors.push_back(
	    VerificationResult{hicann, subsystem, boost::algorithm::join(valid_errors, "\n"),
	                       errors.size(), static_cast<size_t>(valid_errors.size()), reliable,
	                       true});
	if (valid_errors.size() > 0) {
		LOG4CXX_WARN(getLogger(),
		             short_format(hicann)
		                 << " " << subsystem << ": "
		                 << std::string("FAILED") + (reliable ? "" : " (UNRELIABLE)"));
	} else {
		LOG4CXX_INFO(getLogger(), short_format(hicann) << " " << subsystem << ": "
		                                               << "OK");
	}
}

void VerifyConfigurator::post_error(
    HICANNGlobal hicann, std::string subsystem, std::string error, bool reliable)
{
	if (!error.empty()) {
		LOG4CXX_INFO(getLogger(), short_format(hicann) << " " << subsystem << ": "
		                                               << "FAILED");
		mErrors.push_back(
		    VerificationResult{hicann, subsystem, error, 1, 1, reliable, true});
	} else {
		LOG4CXX_INFO(getLogger(), short_format(hicann) << " " << subsystem << ": "
		                                               << "OK");
		mErrors.push_back(
		    VerificationResult{hicann, subsystem, error, 1, 0, reliable, true});
	}
}

log4cxx::LoggerPtr VerifyConfigurator::getLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.VerifyConfigurator");
	return _logger;
}

log4cxx::LoggerPtr VerifyConfigurator::getTimeLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.VerifyConfigurator.Time");
	return _logger;
}

VerifyConfigurator::VerifyConfigurator(bool voe, VerifyConfigurator::SynapsePolicy sp) :
    m_verify_only_enabled(voe), m_synapse_policy(sp)
{
}

void VerifyConfigurator::config_fpga(fpga_handle_t const&, fpga_t const&)
{
	// TODO FPGA read not possible, post msg
}

void VerifyConfigurator::config(
	fpga_handle_t const&, hicann_handle_t const& h, hicann_data_t const& data)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(getLogger(), "read back HICANN: " << h->coordinate());

	read_fg_stimulus(h, data);
	read_analog_readout(h, data);
	read_merger_tree(h, data);
	read_dncmerger(h, data);
	read_gbitlink(h, data);
	read_phase(h, data);

	read_repeater(h, data);
	read_synapse_switch(h, data);
	read_crossbar_switches(h, data);
	read_synapse_drivers(h, data);
	read_synapse_controllers(h, data);
	read_synapse_weights(h, data);
	read_synapse_decoders(h, data);

	read_neuron_config(h, data);
	read_background_generators(h, data);
	// HW-Bug: reading changes stored values, see #818
	// read_neuron_quads(h, data);

	LOG4CXX_INFO(
		getLogger(), "finished reading HICANN " << h->coordinate() << " in " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_floating_gates(
	fpga_handle_t const&, hicann_handle_t const& h, hicann_data_t const&)
{
	// We could read back the voltages from the floating gates, like in the
	// HICANNReadoutConfigurator, but:
	//  * it requires to modify the configuration
	//  * we don't have a good definition, when a value is not good enough
	//  * it would be very slow compared to all other read_* functions
	data_not_readable(h->coordinate(), "floating_gates");
}

void VerifyConfigurator::read_fg_stimulus(hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back current stimuli");
	size_t passes = expected->floating_gates.getNoProgrammingPasses();
	std::array<HMF::HICANN::FGConfig, FGBlockOnHICANN::enum_type::size> config_values;
	std::array<FGStimulus, FGBlockOnHICANN::enum_type::size> current_stimuli;

	// Get the last written FGConfig and the current stimulus values
	// The pulselenght of the current stimulus is determined by the setting in FGConfig.
	// This implies that the FGConfig is updated after set_current_stimulus
	for (auto block : iter_all<FGBlockOnHICANN>()) {
		config_values[block.toEnum()] = ::HMF::HICANN::get_fg_config(*h, block);
		current_stimuli[block.toEnum()] = ::HMF::HICANN::get_current_stimulus(*h, block);
		// Reconstruct pulselength from the FGConfig
		current_stimuli[block.toEnum()].setPulselength(
		    config_values[block.toEnum()].pulselength.to_ulong());
	}

	std::vector<std::string> errors;
	if (passes > 0) {
		for (auto block : iter_all<FGBlockOnHICANN>()) {
			::HMF::HICANN::FGConfig expected_cfg =
				expected->floating_gates.getFGConfig(Enum(passes - 1));
			expected_cfg.pulselength = expected->current_stimuli[block.toEnum()].getPulselength();
			errors.push_back(check(block, expected_cfg, config_values[block.toEnum()]));
		}
	}
	post_merge_errors(h->coordinate(), "fg_config", errors, true);
	errors.clear();
	for (auto block : iter_all<FGBlockOnHICANN>()) {
		errors.push_back(
			check(block, expected->current_stimuli[block.toEnum()], current_stimuli[block.toEnum()]));
	}
	post_merge_errors(h->coordinate(), "current_stimulus", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back current stimuli took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_analog_readout(
	hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back analog output");
	post_error(h->coordinate(), "analog", check(expected->analog, ::HMF::HICANN::get_analog(*h)));
	LOG4CXX_DEBUG(getTimeLogger(), "read back analog output took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_merger_tree(hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back merger tree");
	post_error(
		h->coordinate(), "merger_tree",
		check(expected->layer1.getMergerTree(), ::HMF::HICANN::get_merger_tree(*h)));
	LOG4CXX_DEBUG(getTimeLogger(), "read back merger tree took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_dncmerger(hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back dnc merger");
	post_error(
		h->coordinate(), "dnc_merger",
		check(expected->layer1.getDNCMergerLine(), ::HMF::HICANN::get_dnc_merger(*h)));
	LOG4CXX_DEBUG(getTimeLogger(), "read back dnc merger took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_gbitlink(hicann_handle_t const& h, hicann_data_t const&)
{
	// No getter in halbe
	data_not_readable(h->coordinate(), "gbitlink");
}

void VerifyConfigurator::read_phase(hicann_handle_t const& h, hicann_data_t const&)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	::HMF::HICANN::Phase expected = 0x00; // Hard coded in HICANNBackend.h
	LOG4CXX_DEBUG(getLogger(), "read back phase");
	post_error(h->coordinate(), "phase", check(expected, ::HMF::HICANN::get_phase(*h)));
	LOG4CXX_DEBUG(getTimeLogger(), "read back phase took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_repeater(hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back repeaters");
	const L1Repeaters& repeaters = expected->repeater;

	std::vector<std::string> errors;
	for (auto addr : iter_all<HRepeaterOnHICANN>()) {
		LOG4CXX_TRACE(getLogger(), "read back: " << addr);
		errors.push_back(check(addr, repeaters[addr], ::HMF::HICANN::get_repeater(*h, addr)));
	}
	for (auto addr : iter_all<VRepeaterOnHICANN>()) {
		LOG4CXX_TRACE(getLogger(), "read back: " << addr);
		errors.push_back(check(addr, repeaters[addr], ::HMF::HICANN::get_repeater(*h, addr)));
	}
	for (auto addr : iter_all<RepeaterBlockOnHICANN>()) {
		LOG4CXX_TRACE(getLogger(), "read back: " << addr);
		auto value = ::HMF::HICANN::get_repeater_block(*h, addr);
		// Only SRAM timings are writ- and readable
		errors.push_back(check(addr, repeaters[addr].timings, value.timings));
	}
	post_merge_errors(h->coordinate(), "repeater", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back repeaters took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_synapse_drivers(
	hicann_handle_t const& h, hicann_data_t const& expected_)
{
	const HICANN& expected = dynamic_cast<const HICANN&>(*expected_);
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back synapse drivers");
	std::vector<std::string> errors;
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		::HMF::HICANN::SynapseController synapse_controller =
		    static_cast<HMF::HICANN::SynapseController>(
		        expected.synapse_controllers[syndrv.toSynapseArrayOnHICANN()]);
		LOG4CXX_TRACE(getLogger(), "read back: " << syndrv);
		if (!not_usable(expected, syndrv)) {
			::HMF::HICANN::SynapseDriver expected_synapse_driver = expected.synapses[syndrv];
			::HMF::HICANN::SynapseDriver configured_synapse_driver =
			    ::HMF::HICANN::get_synapse_driver(*h, synapse_controller, syndrv);

			// expected: disabled but configured enabled

			if(m_verify_only_enabled &&
			   !expected_synapse_driver.is_enabled() &&
			   !configured_synapse_driver.is_enabled()) {
				continue;
			}

			errors.push_back(check(syndrv, expected_synapse_driver, configured_synapse_driver));
		} else {
			errors.push_back(std::string());
		}
	}
	post_merge_errors(h->coordinate(), "synapse_drivers", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back synapse drivers took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_synapse_weights(
	hicann_handle_t const& h, hicann_data_t const& expected_)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back synapses weights");
	std::vector<std::string> errors;

	const HICANN& expected = dynamic_cast<const HICANN&>(*expected_);

	SynapseArray values;
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {

		::HMF::HICANN::SynapseDriver expected_synapse_driver = expected.synapses[syndrv];

		::HMF::HICANN::SynapseController const& synapse_controller =
		    static_cast<HMF::HICANN::SynapseController>(
		        expected.synapse_controllers[syndrv.toSynapseArrayOnHICANN()]);

		::HMF::HICANN::SynapseDriver configured_synapse_driver =
			::HMF::HICANN::get_synapse_driver(*h, synapse_controller, syndrv);

		if(m_verify_only_enabled &&
		   !expected_synapse_driver.is_enabled() &&
		   !configured_synapse_driver.is_enabled()) {
			continue;
		}

		for (auto side_vertical : iter_all<SideVertical>()) {
			SynapseRowOnHICANN row(syndrv, RowOnSynapseDriver(side_vertical));
			LOG4CXX_TRACE(getLogger(), "read back: " << row);
			::HMF::HICANN::WeightRow const weights =
				  ::HMF::HICANN::get_weights_row(*h, synapse_controller, row);
			for (auto column : iter_all<SynapseColumnOnHICANN>()) {
				SynapseOnHICANN synapse(row, column);
				if (!not_usable(expected, synapse)) {
					std::stringstream prefix;
					prefix << synapse << " (" << synapse.toSynapseDriverOnHICANN() << ", "
					       << synapse.toSynapseRowOnHICANN() << ", "
					       << synapse.toSynapseColumnOnHICANN() << ", "
					       << synapse.toNeuronOnHICANN() << ")";
					switch (m_synapse_policy) {
						case SynapsePolicy::All:
							errors.push_back(check(
							    prefix.str(), expected.synapses[synapse].weight, weights[column]));
							break;
						case SynapsePolicy::Mask:
							errors.push_back(check(
							    prefix.str(), expected.synapses[synapse].weight, weights[column],
							    m_synapse_mask, SynapseOnWafer(synapse, h->coordinate())));
							break;
						case SynapsePolicy::None:
							errors.push_back(std::string());
							break;
						default:
							LOG4CXX_ERROR(
							    getLogger(),
							    "Unknown synapse policy. Choose one of All, Mask or None.");
							throw std::runtime_error("Unknown synapse policy.");
					}
				} else {
					errors.push_back(std::string());
				}
			}
		}
	}
	post_merge_errors(h->coordinate(), "synapse_weights", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back synapses weights took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_synapse_decoders(
	hicann_handle_t const& h, hicann_data_t const& expected_)
{
	const HICANN& expected = dynamic_cast<const HICANN&>(*expected_);
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back synapses decoders");


	std::vector<std::string> errors;
	SynapseArray values;
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		::HMF::HICANN::SynapseController const& synapse_controller =
		    static_cast<HMF::HICANN::SynapseController>(
		        expected.synapse_controllers[syndrv.toSynapseArrayOnHICANN()]);

		LOG4CXX_TRACE(getLogger(), "read back: " << syndrv);
		values.setDecoderDoubleRow(
			syndrv, ::HMF::HICANN::get_decoder_double_row(*h, synapse_controller, syndrv));
	}

	halco::common::typed_array<
	    std::pair< ::HMF::HICANN::SynapseDriver, ::HMF::HICANN::SynapseDriver>,
	    SynapseDriverOnHICANN>
	    syndrvs;

	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		::HMF::HICANN::SynapseController synapse_controller =
		    static_cast<HMF::HICANN::SynapseController>(
		        expected.synapse_controllers[syndrv.toSynapseArrayOnHICANN()]);

		::HMF::HICANN::SynapseDriver expected_synapse_driver = expected.synapses[syndrv];
		::HMF::HICANN::SynapseDriver configured_synapse_driver =
		    ::HMF::HICANN::get_synapse_driver(*h, synapse_controller, syndrv);
		syndrvs[syndrv].first = expected_synapse_driver;
		syndrvs[syndrv].second = configured_synapse_driver;
	}

	for (auto synapse : iter_all<SynapseOnHICANN>()) {
		auto const syndrv = synapse.toSynapseDriverOnHICANN();

		auto const expected_synapse_driver = syndrvs[syndrv].first;
		auto const configured_synapse_driver = syndrvs[syndrv].second;

		if (m_verify_only_enabled && !expected_synapse_driver.is_enabled() &&
		    !configured_synapse_driver.is_enabled()) {
			continue;
		}

		if (!not_usable(expected, synapse)) {
			errors.push_back(
			    check(synapse, expected.synapses[synapse].decoder, values[synapse].decoder));
		} else {
			errors.push_back(std::string());
		}
	}
	post_merge_errors(h->coordinate(), "synapse_decoders", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back synapses decoder took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_synapse_controllers
	(hicann_handle_t const& h, hicann_data_t const& expected_)
{
	SynapseControllers const& expected_controllers = expected_->synapse_controllers;
	SynapseControllers read_controllers;

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": read back synapse controller");

	std::vector<std::string> errors;
	for (auto addr : iter_all<SynapseArrayOnHICANN>()) {
		read_controllers[addr] =  SynapseControllerData(
		    ::HMF::HICANN::get_synapse_controller(*h, addr));
		errors.push_back(check(addr, expected_controllers[addr], read_controllers[addr]));
	}

	post_merge_errors(h->coordinate(), "synapse controllers", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back synapse controller took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_synapse_switch(
	hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back L1 synapse switches");

	SynapseSwitches values;
	for (auto row : iter_all<SynapseSwitchRowOnHICANN>()) {
		for (auto side : iter_all<Side>()) {
			LOG4CXX_TRACE(getLogger(), "read back: " << row << "(" << side << ")");
			values.set_row(row, ::HMF::HICANN::get_syndriver_switch_row(*h, row));
		}
	}

	std::vector<std::string> errors;
	for (auto row : iter_all<SynapseSwitchRowOnHICANN>()) {
		for (auto column : values.get_lines(row)) {
			errors.push_back(check(
				make_xy(column, row), expected->synapse_switches.get(column, row.y()),
				values.get(column, row.y())));
		}
	}
	post_merge_errors(h->coordinate(), "l1_synapse_switches", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back L1 synapse switches took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_crossbar_switches(
	hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back L1 crossbar switches");

	CrossbarSwitches values;
	for (auto row : iter_all<HLineOnHICANN>()) {
		for (auto side : iter_all<Side>()) {
			LOG4CXX_TRACE(getLogger(), "read back: " << row << "(" << side << ")");
			values.set_row(row, side, ::HMF::HICANN::get_crossbar_switch_row(*h, row, side));
		}
	}

	std::vector<std::string> errors;
	for (auto row : iter_all<HLineOnHICANN>()) {
		for (auto column : values.get_lines(row)) {
			errors.push_back(check(
				make_xy(column, row), expected->crossbar_switches.get(column, row),
				values.get(column, row)));
		}
	}
	post_merge_errors(h->coordinate(), "l1_crossbar_switches", errors, true);
	LOG4CXX_DEBUG(getTimeLogger(), "read back L1 crossbar switches took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_neuron_config(hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back neuron config");
	post_error(
		h->coordinate(), "neuron_config",
		check(expected->neurons.config, ::HMF::HICANN::get_neuron_config(*h)));
	LOG4CXX_DEBUG(getTimeLogger(), "read back neuron config took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_neuron_quads(hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back denmem quads");
	std::vector<std::string> errors;
	for (auto quad : iter_all<QuadOnHICANN>()) {
		LOG4CXX_TRACE(getLogger(), "read back: " << quad);
		errors.push_back(
			check(quad, expected->neurons[quad], ::HMF::HICANN::get_denmem_quad(*h, quad)));
	}
	post_merge_errors(h->coordinate(), "denmem_quads", errors, false);
	LOG4CXX_DEBUG(getTimeLogger(), "read back denmem quads took " << t.get_ms() << "ms");
}

void VerifyConfigurator::read_background_generators(
	hicann_handle_t const& h, hicann_data_t const& expected)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back background generators");
	post_error(
		h->coordinate(), "dnc_merger",
		check(
			expected->layer1.getBackgroundGeneratorArray(),
			::HMF::HICANN::get_background_generator(*h)));
	LOG4CXX_DEBUG(getTimeLogger(), "read back background generators took " << t.get_ms() << "ms");
}

void VerifyConfigurator::clear()
{
	mErrors.clear();
}

std::vector<VerificationResult> VerifyConfigurator::results() const
{
	return std::vector<VerificationResult>(mErrors.begin(), mErrors.end());
}

size_t VerifyConfigurator::error_count(bool include_unreliable) const
{
	size_t errors = 0;
	for (auto result : results()) {
		if (result.readable && (result.reliable || include_unreliable)) {
			errors += result.errors;
		}
	}
	return errors;
}

std::ostream& operator<<(std::ostream& out, const VerifyConfigurator& cfg)
{
	out << "VerifyConfigurator:";
	for (auto result : cfg.results()) {
		out << "\n    " << result;
	}
	return out;
}

void VerifyConfigurator::set_synapse_policy(VerifyConfigurator::SynapsePolicy const sp)
{
	m_synapse_policy = sp;
}

VerifyConfigurator::SynapsePolicy VerifyConfigurator::get_synapse_policy() const
{
	return m_synapse_policy;
}

void VerifyConfigurator::set_synapse_mask(std::vector<SynapseOnWafer> const& sw)
{
	m_synapse_mask = sw;
}

std::vector<SynapseOnWafer> VerifyConfigurator::get_synapse_mask() const
{
	return m_synapse_mask;
}

} // end namespace sthal
