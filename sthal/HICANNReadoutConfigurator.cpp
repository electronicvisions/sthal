#include "sthal/HICANNReadoutConfigurator.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/variance.hpp>

#include "hal/backend/HICANNBackend.h"
#include "hal/backend/FPGABackend.h"
#include "hal/backend/DNCBackend.h"
#include "hal/HICANN/FGBlock.h"
#include "hal/Handle/HICANN.h"
#include "hal/Handle/FPGA.h"
#include "hal/Coordinate/iter_all.h"

#include "sthal/HICANN.h"
#include "sthal/FPGA.h"
#include "sthal/Timer.h"
#include "sthal/AnalogRecorder.h"
#include "sthal/Wafer.h"

#include <log4cxx/logger.h>

using namespace ::HMF::Coordinate;

namespace sthal {

log4cxx::LoggerPtr HICANNReadoutConfigurator::getLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.HICANNReadoutConfigurator");
	return _logger;
}

log4cxx::LoggerPtr HICANNReadoutConfigurator::getTimeLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.HICANNReadoutConfigurator.Time");
	return _logger;
}

HICANNReadoutConfigurator::HICANNReadoutConfigurator(
    Wafer& wafer_, std::vector<HICANNOnWafer> read_hicanns_)
    : wafer(wafer_), read_hicanns(read_hicanns_.begin(), read_hicanns_.end())
{
}

void HICANNReadoutConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(getLogger(), "read FPGA: " << f->coordinate());
	read_dnc_link(f, fpga);
	LOG4CXX_INFO(
		getLogger(),
		"finished reading configuring FPGA " << f->coordinate() << " in " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::config(
	fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& data)
{

	if (!read_hicanns.empty() && read_hicanns.find(h->coordinate()) == read_hicanns.end()) {
		LOG4CXX_DEBUG(getLogger(), h->coordinate()
		                               << " is not the HICANN we are looking for");
		return;
	}

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);

	LOG4CXX_INFO(getLogger(), "read back HICANN: " << h->coordinate());

	HICANN& hicann = wafer[h->coordinate()];
	read_fg_stimulus(h, hicann);
	read_analog_readout(h, hicann);
	read_merger_tree(h, hicann);
	read_dncmerger(h, hicann);
	read_gbitlink(h, hicann);
	read_phase(h, hicann);

	read_repeater(h, hicann);
	read_synapse_switch(h, hicann);
	read_crossbar_switches(h, hicann);
	read_synapse_drivers(h, hicann);
	read_synapse_array(h, hicann);
	read_stdp(h, hicann);
	read_neuron_config(h, hicann);
	read_background_generators(h, hicann);
	read_neuron_quads(h, hicann);

	read_floating_gates(f, h, hicann, data);

	LOG4CXX_INFO(
		getLogger(), "finished reading HICANN " << h->coordinate() << " in " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_dnc_link(fpga_handle_t const& f, fpga_t const& original_fpga)
{
	FPGA& fpga = wafer[original_fpga->coordinate()];
	for (auto dnc : iter_all<DNCOnFPGA>() )
	{
		LOG4CXX_INFO(getLogger(), "read DNC " << dnc);
		if (!f->dnc_active(dnc))
			continue;

		// TODO if possible!
		HMF::DNC::GbitReticle gbit;
		// = ::HMF::DNC::get_hicann_directions(*f, dnc);

		for (auto hicann : iter_all<HICANNOnDNC>() )
		{
			const auto & h = fpga[dnc][hicann];
			if (h)
			{
				h->layer1.mGbitLink = gbit[hicann];
			}
		}
	}
}

void HICANNReadoutConfigurator::read_floating_gates(
	fpga_handle_t const&,
	hicann_handle_t const& h,
	HICANN& hicann,
	hicann_data_t const& original_hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	FloatingGates fg;

	for (auto block : iter_all<FGBlockOnHICANN>())
	{
		::HMF::HICANN::FGBlock & block_data = fg[block];
		AnalogOnHICANN analog;
        ::HMF::HICANN::Analog ac;
        if (block.y() == top)
            analog = AnalogOnHICANN(0);
		else
            analog = AnalogOnHICANN(1);
        if (block.x() == left)
            ac.set_fg_left(analog);
        else
            ac.set_fg_right(analog);

		::HMF::HICANN::set_analog(*h, ac);
		AnalogRecorder record =
			dynamic_cast<HICANN&>(*original_hicann).analogRecorder(analog);
		Timer t_block;
		for (auto cell : iter_all<FGCellOnFGBlock>())
		{
			namespace ba = boost::accumulators;
			::HMF::HICANN::set_fg_cell(*h, block, cell);
			flush_hicann(h);
			ba::accumulator_set<AnalogRecorder::voltage_type,
			                    ba::stats<ba::tag::mean, ba::tag::variance> > acc;
			record.record(10e-6); // ~960 samples
			for (AnalogRecorder::voltage_type value : record.trace())
			{
				acc(value);
			}
			AnalogRecorder::voltage_type mean_value = ba::mean(acc);
			::HMF::HICANN::FGBlock::value_type value = mean_value / 1.8f * 1023.0;
			LOG4CXX_TRACE(getLogger(), "Measured: " << mean_value << " +- "
									   << ba::variance(acc) << "mV = "
									   << value << "DAC for cell " << cell);
			block_data.setRaw(cell, value);
		}
		LOG4CXX_INFO(getTimeLogger(), "reading " << block << " took "
									  << t_block.get_ms() << "ms");
	}
	hicann.floating_gates = fg;
	LOG4CXX_DEBUG(getTimeLogger(),
			      "reading FG blocks took " << t.get_ms() << "ms");

	// Have to restore analog readout after reading floating gates
	LOG4CXX_DEBUG(getLogger(),
			      "restoring changed stuff" << t.get_ms() << "ms");
	config_fg_stimulus(h, original_hicann);
	config_analog_readout(h, original_hicann);
}

void HICANNReadoutConfigurator::read_fg_stimulus(hicann_handle_t const& h, HICANN& hicann)
{
	// We reuse the configuration of the floating gate controller from the
	// last write and change only the pulselenght.

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back current stimuli");

	FloatingGates & fg = hicann.floating_gates;
	size_t passes = fg.getNoProgrammingPasses();
	std::array< ::HMF::HICANN::FGConfig, FGBlockOnHICANN::enum_type::size> cfgs;
	for (auto block : iter_all<FGBlockOnHICANN>() )
	{
		if (passes > 0)
		{
			cfgs[block.toEnum()] = ::HMF::HICANN::get_fg_config(*h, block);
		}
		hicann.current_stimuli[block.toEnum()] = ::HMF::HICANN::get_current_stimulus(*h, block);
	}
	if (std::adjacent_find(cfgs.begin(), cfgs.end(),
	    std::not_equal_to< ::HMF::HICANN::FGConfig >()))
	{
		LOG4CXX_ERROR(getLogger(),
				"The read FGConfigs are not equal for all 4 blocks! "
				<< "using values from Block 0. Read:\n "
				<< FGBlockOnHICANN(Enum(0)) << ": " << cfgs[0] << "\n"
				<< FGBlockOnHICANN(Enum(1)) << ": " << cfgs[1] << "\n"
				<< FGBlockOnHICANN(Enum(2)) << ": " << cfgs[2] << "\n"
				<< FGBlockOnHICANN(Enum(3)) << ": " << cfgs[3]);
	}
	fg.setFGConfig(Enum(passes-1), cfgs[0]);
	LOG4CXX_DEBUG(getTimeLogger(),
			      "read back current stimuli took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_analog_readout(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back analog output");
	hicann.analog = ::HMF::HICANN::get_analog(*h);
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back analog output took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_merger_tree(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back merger tree");
	hicann.layer1.setMergerTree(::HMF::HICANN::get_merger_tree(*h));
	LOG4CXX_DEBUG(getTimeLogger(), "read back merger tree took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_dncmerger(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back dnc merger");
	hicann.layer1.setDNCMergerLine(::HMF::HICANN::get_dnc_merger(*h));
	LOG4CXX_DEBUG(getTimeLogger(),
		   "read back dnc merger took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_gbitlink(hicann_handle_t const&, HICANN&)
{
	LOG4CXX_INFO(getLogger(), "cannot read back GbitLink");
}

void HICANNReadoutConfigurator::read_phase(hicann_handle_t const&, HICANN&)
{
	LOG4CXX_INFO(getLogger(), "cannot read back phase");
}

void HICANNReadoutConfigurator::read_repeater(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back repeaters");
	L1Repeaters & repeaters = hicann.repeater;

	// configure sending & horizontal repeater
	for (auto addr : iter_all<HRepeaterOnHICANN>() )
	{
		LOG4CXX_TRACE(getLogger(), "read back horizontal repeater: " << addr);
		repeaters[addr] = ::HMF::HICANN::get_repeater(*h, addr);
	}

	// configure vertical repeater
	for (auto addr : iter_all<VRepeaterOnHICANN>() )
	{
		LOG4CXX_TRACE(getLogger(), "read back vertical repeater: " << addr);
		repeaters[addr] = ::HMF::HICANN::get_repeater(*h, addr);
	}

	// configure repeater block
	for (auto addr : iter_all<RepeaterBlockOnHICANN>() )
	{
		LOG4CXX_TRACE(getLogger(), "read back repeater block: " << addr);
		repeaters.setRepeaterBlock(addr, ::HMF::HICANN::get_repeater_block(*h, addr));
	}
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back repeaters took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_synapse_drivers(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back synapse drivers");
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>() )
	{
		hicann.synapses[syndrv] = ::HMF::HICANN::get_synapse_driver(*h, syndrv);
	}
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back synapse drivers took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_synapse_array(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back synapses");
	for (auto syndrv : iter_all<SynapseDriverOnHICANN>() )
	{
		hicann.synapses.setDecoderDoubleRow(
			syndrv, ::HMF::HICANN::get_decoder_double_row(*h, syndrv));

		for (auto side : iter_all<SideVertical>())
		{
			SynapseRowOnHICANN row(syndrv, RowOnSynapseDriver(side));
			hicann.synapses[row].weights = ::HMF::HICANN::get_weights_row(*h, row);
		}
	}
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back synapses took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_stdp(hicann_handle_t const&, HICANN&)
{
	LOG4CXX_INFO(getLogger(), "cannot read back STDP");
}

void HICANNReadoutConfigurator::read_synapse_switch(hicann_handle_t const& h, HICANN& hicann)
{
	typedef SynapseSwitchRowOnHICANN coord;

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back synapse switches");

	for (coord::enum_type::value_type ii = 0; ii < coord::enum_type::end; ++ii)
	{
		SynapseSwitchRowOnHICANN row{Enum{ii}};
		hicann.synapse_switches.set_row(row, ::HMF::HICANN::get_syndriver_switch_row(*h, row));
	}
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back synapse switches took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_crossbar_switches(hicann_handle_t const& h, HICANN& hicann)
{
	typedef HLineOnHICANN coord;

	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back crossbar switches");
	for (coord::value_type ii = 0; ii < coord::end; ++ii)
	{
		coord row{ii};
		hicann.crossbar_switches.set_row(
			row, left, ::HMF::HICANN::get_crossbar_switch_row(*h, row, left));
		hicann.crossbar_switches.set_row(
			row, right, ::HMF::HICANN::get_crossbar_switch_row(*h, row, right));
	}
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back crossbar switches took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_neuron_config(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back neuron config");
	hicann.neurons.config = ::HMF::HICANN::get_neuron_config(*h);
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back neuron config took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_neuron_quads(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back denmem quads");
	for (auto quad : iter_all<QuadOnHICANN>() )
	{
		hicann.neurons[quad] = ::HMF::HICANN::get_denmem_quad(*h, quad);
	}
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back denmem quads took " << t.get_ms() << "ms");
}

void HICANNReadoutConfigurator::read_background_generators(hicann_handle_t const& h, HICANN& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_DEBUG(getLogger(), "read back background generators");
	hicann.layer1.setBackgroundGeneratorArray(::HMF::HICANN::get_background_generator(*h));
	LOG4CXX_DEBUG(getTimeLogger(),
			"read back background generators took " << t.get_ms() << "ms");
}

} // end namespace sthal
