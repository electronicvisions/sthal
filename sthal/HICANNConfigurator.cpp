#include "sthal/HICANNConfigurator.h"

#include "hal/backend/HICANNBackend.h"
#include "hal/backend/FPGABackend.h"
#include "hal/backend/DNCBackend.h"
#include "hal/Handle/HICANN.h"
#include "hal/Handle/FPGA.h"
#include "hal/Coordinate/iter_all.h"
#include "hal/Coordinate/FormatHelper.h"

#include "sthal/HICANN.h"
#include "sthal/FPGA.h"
#include "sthal/Timer.h"

#include <log4cxx/logger.h>

using namespace ::HMF::Coordinate;

namespace sthal {

log4cxx::LoggerPtr HICANNConfigurator::getLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.HICANNConfigurator");
	return _logger;
}

log4cxx::LoggerPtr HICANNConfigurator::getTimeLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.HICANNConfigurator.Time");
	return _logger;
}

HICANNConfigurator::HICANNConfigurator()
{
}

HICANNConfigurator::~HICANNConfigurator() noexcept(true)
{
}

void HICANNConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(getLogger(), "reset FPGA: " << short_format(f->coordinate()));
	::HMF::FPGA::Reset r;
	r.PLL_frequency = static_cast<uint8_t>(fpga->commonFPGASettings()->getPLL() / 1.0e6);
	::HMF::FPGA::reset(*f, r);
	// initialize all HICANNs without zeroing the synapse array
	::HMF::FPGA::init(*f, false);

	if (fpga->getSpinnakerEnable())
		config_spinnaker_interface(f, fpga);

	config_dnc_link(f, fpga);
	LOG4CXX_INFO(getLogger(), "finished configuring FPGA "
	                              << short_format(f->coordinate()) << " in " << t.get_ms()
	                              << "ms");
}

void HICANNConfigurator::ensure_correct_l1_init_settings(
	hicann_handle_t const&, hicann_data_t const& hicann)
{
	// check repeater blocks
	for (auto addr : iter_all<RepeaterBlockOnHICANN>()) {
		if (hicann->repeater[addr].dllresetb != false) {
			hicann->repeater[addr].dllresetb = false;
			LOG4CXX_WARN(getLogger(),
			    addr << ": Overwriting dllreset to active during initialization.");
		}
		if (hicann->repeater[addr].drvresetb != true) {
			hicann->repeater[addr].drvresetb = true;
			LOG4CXX_WARN(getLogger(),
			    addr << ": Overwriting drvreset to inactive during initialization.");
		}
	}

	// check synapse controllers
	for (auto addr : iter_all<SynapseArrayOnHICANN>()) {
		HMF::HICANN::SynapseController& synapse_controller = hicann->synapse_controllers[addr];
		HMF::HICANN::SynapseControlRegister& ctrl_reg = synapse_controller.ctrl_reg;
		HMF::HICANN::SynapseConfigurationRegister& cnfg_reg = synapse_controller.cnfg_reg;

		if (ctrl_reg.cmd != ::HMF::HICANN::SynapseControlRegister::Opcodes::IDLE) {
			throw std::runtime_error("Command in synapse controller has to be IDLE in order to "
			                         "perform initialization correctly");
		}

		// DLL reset enabled for all drivers on synapse array
		if (cnfg_reg.dllresetb != ::HMF::HICANN::SynapseDllresetb::min) {
			cnfg_reg.dllresetb =
				::HMF::HICANN::SynapseDllresetb(::HMF::HICANN::SynapseDllresetb::min);
			LOG4CXX_WARN(getLogger(),
			    addr << ": Overwriting dllreset of the synapse controller to active"
				        " during initialization.");
		}
	}
}

void HICANNConfigurator::init_controllers(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	config_neuron_config(h, hicann);

	// check for correct initial configuration of repeaters and synapse controllers
	ensure_correct_l1_init_settings(h, hicann);

	config_repeater_blocks(h, hicann);
	config_synapse_controllers(h, hicann);
}

void HICANNConfigurator::prime_systime_counter(fpga_handle_t const& f)
{
	::HMF::FPGA::prime_systime_counter(*f);
}

void HICANNConfigurator::start_systime_counter(fpga_handle_t const& f)
{
	::HMF::FPGA::start_systime_counter(*f);
}

void HICANNConfigurator::disable_global(fpga_handle_t const& f)
{
	::HMF::FPGA::disable_global(*f);
}

void HICANNConfigurator::config_spinnaker_interface(fpga_handle_t const& f, fpga_t const& fpga)
{
	LOG4CXX_INFO(getLogger(),
	             "configuring SpiNNaker interface: " << short_format(fpga->coordinate()));

	::HMF::FPGA::set_spinnaker_pulse_upsampler(*f, fpga->getSpinnakerUpsampleCount());
	::HMF::FPGA::set_spinnaker_pulse_downsampler(*f, fpga->getSpinnakerDownsampleCount());
	::HMF::FPGA::set_spinnaker_routing_table(*f, fpga->getSpinnakerRoutingTable());
}

void HICANNConfigurator::config_dnc_link(fpga_handle_t const& f, fpga_t const& fpga)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	for (auto dnc : iter_all<DNCOnFPGA>() )
	{
		if (!f->dnc_active(dnc))
			continue;

		HMF::DNC::GbitReticle gbit;
		for (auto hicann : iter_all<HICANNOnDNC>() )
		{
			const auto& h = (*fpga)[dnc][hicann];
			if (h)
			{
				gbit[hicann] = h->layer1.mGbitLink;
			}
		}
		LOG4CXX_INFO(getLogger(), "configure DNC " << dnc << ": "
		                                           << short_format(fpga->coordinate()));
		::HMF::DNC::set_hicann_directions(*f, dnc, gbit);
	}
}

void HICANNConfigurator::config(
	fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(getLogger(), "configure HICANN: " << short_format(h->coordinate()));

	init_controllers(h, hicann);

	config_floating_gates(h, hicann);
	config_fg_stimulus(h, hicann);

	config_synapse_array(h, hicann);

	config_neuron_quads(h, hicann, true);
	config_phase(h, hicann);
	config_gbitlink(h, hicann);

	config_synapse_switch(h, hicann);
	config_stdp(h, hicann);
	config_crossbar_switches(h, hicann);
	config_repeater(h, hicann);
	sync_command_buffers(f, hicann_handles_t{h});

	hicann->repeater.disable_dllreset();
	config_repeater_blocks(h, hicann);
	sync_command_buffers(f, hicann_handles_t{h});

	config_merger_tree(h, hicann);
	config_dncmerger(h, hicann);
	config_background_generators(h, hicann);
	flush_hicann(h);
	config_synapse_drivers(h, hicann);

	hicann->synapse_controllers.disable_dllreset();
	config_synapse_controllers(h, hicann);
	sync_command_buffers(f, hicann_handles_t{h});

	config_neuron_config(h, hicann);
	config_neuron_quads(h, hicann);
	config_analog_readout(h, hicann);

	LOG4CXX_INFO(getLogger(), "finished configuring HICANN "
	                              << short_format(h->coordinate()) << " in " << t.get_ms()
	                              << "ms");
}

void HICANNConfigurator::flush_hicann(hicann_handle_t const& h)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": flush HICANN");
	::HMF::HICANN::flush(*h);
}

void HICANNConfigurator::config_floating_gates(
	hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	const FloatingGates& fg = hicann->floating_gates;
	size_t passes = fg.getNoProgrammingPasses();
	for (size_t pass = 0; pass < passes; ++pass)
	{
		LOG4CXX_DEBUG(getLogger(), "writing FG blocks (pass " << pass + 1
				                   << " out of " << passes << "): ");
		FGConfig cfg = fg.getFGConfig(Enum(pass));
		for (auto block : iter_all<FGBlockOnHICANN>())
		{
			::HMF::HICANN::set_fg_config(*h, block, cfg);
		}
		for (auto row : iter_all<FGRowOnFGBlock>())
		{
			write_fg_row(h, row, fg, cfg.writeDown);
		}
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": writing FG blocks took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::write_fg_row(
	hicann_handle_t const& h, const FGRowOnFGBlock& row, const FloatingGates& fg, bool writeDown)
{
	::HMF::HICANN::set_fg_row_values(*h, row, fg, writeDown);
}

void HICANNConfigurator::config_fg_stimulus(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	// We reuse the configuration of the floating gate controller from the
	// last write and change only the pulselenght.

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": writing current stimuli");

	const FloatingGates& fg = hicann->floating_gates;
	size_t passes = fg.getNoProgrammingPasses();
	for (auto block : iter_all<FGBlockOnHICANN>() )
	{
		::HMF::HICANN::FGConfig cfg;
		if (passes > 0)
			cfg = fg.getFGConfig(Enum(passes-1));
		FGStimulus stim = hicann->current_stimuli[block.id()];
		cfg.pulselength = stim.getPulselength();
		::HMF::HICANN::set_fg_config(*h, block, cfg);
		::HMF::HICANN::set_current_stimulus(*h, block, stim);
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": writing current stimuli took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_analog_readout(
	hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure analog output");
	::HMF::HICANN::set_analog(*h, hicann->analog);
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure analog output took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_merger_tree(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure merger tree");
	::HMF::HICANN::set_merger_tree(*h, hicann->layer1.getMergerTree());
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure merger tree took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_dncmerger(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure dnc merger");
	::HMF::HICANN::set_dnc_merger(*h, hicann->layer1.getDNCMergerLine());
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure dnc merger took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_gbitlink(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure GbitLink");
	::HMF::HICANN::set_gbit_link(*h, hicann->layer1.getGbitLink());
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure GbitLink took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_phase(hicann_handle_t const& h, hicann_data_t const& /* hicann */)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure phase");
	::HMF::HICANN::set_phase(*h);
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure phase took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_repeater_blocks(
    hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure repeater blocks");

	for (auto addr : iter_all<RepeaterBlockOnHICANN>()) {
		::HMF::HICANN::set_repeater_block(*h, addr, hicann->repeater[addr]);
	}

	LOG4CXX_DEBUG(
	    getTimeLogger(), short_format(h->coordinate())
	                         << ": configure repeater blocks took " << t.get_ms() << "ms");
}

void HICANNConfigurator::config_repeater(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure repeaters");
	const L1Repeaters& repeaters = hicann->repeater;

	// configure sending & horizontal repeater
	for (auto addr : iter_all<HRepeaterOnHICANN>()) {
		LOG4CXX_TRACE(getLogger(), short_format(h->coordinate())
		                               << ": configure horizontal repeater: " << addr);
		::HMF::HICANN::set_repeater(*h, addr, repeaters[addr]);
	}

	// configure vertical repeater
	for (auto addr : iter_all<VRepeaterOnHICANN>()) {
		LOG4CXX_TRACE(getLogger(), short_format(h->coordinate())
		                               << ": configure vertical repeater: " << addr);
		::HMF::HICANN::set_repeater(*h, addr, repeaters[addr]);
	}

	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure repeaters took " << t.get_ms() << "ms");
}

void HICANNConfigurator::config_synapse_drivers(
	hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure synapse drivers");
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		::HMF::HICANN::set_synapse_driver(*h, syndrv, hicann->synapses[syndrv]);
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure synapse drivers took "
	                                   << t.get_ms() << "ms");
}

static std::string format_debug(SynapseRowOnHICANN row_coordinate, const ::HMF::HICANN::WeightRow row)
{
	static char const * const W = "0123456789ABCDEF";
	std::stringstream out;
	out << row_coordinate << "\n";
	for (size_t ii = 0; ii < row.size(); ++ii)
	{
		out << W[row[ii].value()];
		if ((ii + 1) % 16 == 0)
			out << ' ';
	}
	return out.str();
}

void HICANNConfigurator::config_synapse_controllers(
    hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure synapse controllers");

	for (auto addr : iter_all<SynapseArrayOnHICANN>()) {
		::HMF::HICANN::set_synapse_controller(*h, addr, hicann->synapse_controllers[addr]);
	}

	LOG4CXX_DEBUG(
	    getTimeLogger(), short_format(h->coordinate())
	                         << ": configure synapse controllers took " << t.get_ms() << "ms");
}

void HICANNConfigurator::config_synapse_array(hicann_handle_t const& h,
                                              hicann_data_t const& hicann) {
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure synapses");
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>()) {
		::HMF::HICANN::set_decoder_double_row(
		    *h, syndrv, hicann->synapses.getDecoderDoubleRow(syndrv));

		for (auto side : iter_all<SideVertical>()) {
			SynapseRowOnHICANN row(syndrv, RowOnSynapseDriver(side));
			LOG4CXX_TRACE(getLogger(), format_debug(row, hicann->synapses[row].weights));
			::HMF::HICANN::set_weights_row(*h, row, hicann->synapses[row].weights);
		}
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure synapses took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_stdp(hicann_handle_t const& h, hicann_data_t const&) {
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate()) << ": configure STDP");
	LOG4CXX_DEBUG(getLogger(),
	              short_format(h->coordinate())
	                  << ": just kidding, STDP is currently not supported."); // TODO
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure STDP took " << t.get_ms() << "ms");
}

void HICANNConfigurator::config_synapse_switch(hicann_handle_t const& h,
                                               hicann_data_t const& hicann) {
	typedef SynapseSwitchRowOnHICANN coord;

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure synapse switches");

	for (coord::enum_type::value_type ii = 0; ii < coord::enum_type::end; ++ii) {
		SynapseSwitchRowOnHICANN row{Enum{ii}};
		::HMF::HICANN::set_syndriver_switch_row(*h, row,
		                                        hicann->synapse_switches.get_row(row));
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure synapse switches took "
	                                   << t.get_ms() << "ms");
}

void HICANNConfigurator::config_crossbar_switches(hicann_handle_t const& h,
                                                  hicann_data_t const& hicann) {
	typedef HLineOnHICANN coord;

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure crossbar switches");
	for (coord::value_type ii = 0; ii < coord::end; ++ii) {
		coord row{ii};
		::HMF::HICANN::set_crossbar_switch_row(
		    *h, row, left, hicann->crossbar_switches.get_row(row, left));
		::HMF::HICANN::set_crossbar_switch_row(
		    *h, row, right, hicann->crossbar_switches.get_row(row, right));
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure crossbar switches took "
	                                   << t.get_ms() << "ms");
}

void HICANNConfigurator::config_neuron_config(hicann_handle_t const& h,
                                              hicann_data_t const& hicann) {
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure neuron config");
	::HMF::HICANN::set_neuron_config(*h, hicann->neurons.config);
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure neuron config took " << t.get_ms()
	                                   << "ms");
}

void HICANNConfigurator::config_neuron_quads(hicann_handle_t const& h,
                                             hicann_data_t const& hicann,
                                             bool disable_spl1_output) {
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure denmem quads "
	                               << (disable_spl1_output ? " (spl1 disabled) " : ""));
	for (auto quad : iter_all<QuadOnHICANN>()) {
		auto data = hicann->neurons[quad];
		if (disable_spl1_output) {
			for (auto neuron : iter_all<NeuronOnQuad>())
				data[neuron].enable_spl1_output(false);
		}
		::HMF::HICANN::set_denmem_quad(*h, quad, data);
	}
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure denmem quads "
	                                   << (disable_spl1_output ? " (spl1 disabled) " : "")
	                                   << "took " << t.get_ms() << "ms");
}

void HICANNConfigurator::config_background_generators(hicann_handle_t const& h,
                                                      hicann_data_t const& hicann) {
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), short_format(h->coordinate())
	                               << ": configure background generators");
	::HMF::HICANN::set_background_generator(*h,
	                                        hicann->layer1.getBackgroundGeneratorArray());
	LOG4CXX_DEBUG(getTimeLogger(), short_format(h->coordinate())
	                                   << ": configure background generators took "
	                                   << t.get_ms() << "ms");
}

void HICANNConfigurator::sync_command_buffers(fpga_handle_t const& fpga_handle,
                                              hicann_handles_t const& hicann_handles)
{
	auto const t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	// Make sure no commands are pending
	LOG4CXX_DEBUG(getLogger(), short_format(fpga_handle->coordinate())
	                           << ": sync command buffers (Host-FPGA and FPGA-HICANN");

	::HMF::FPGA::flush(*fpga_handle);

	for (auto handle : hicann_handles) {
		flush_hicann(handle);
	}
	LOG4CXX_DEBUG(getTimeLogger(), "synchronization of command buffers took " << t.get_ms() << "ms");
}

} // end namespace sthal
