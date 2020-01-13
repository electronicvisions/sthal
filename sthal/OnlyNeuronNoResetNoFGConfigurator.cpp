#include "sthal/OnlyNeuronNoResetNoFGConfigurator.h"

#include <log4cxx/logger.h>

#include "sthal/Timer.h"
#include "hal/Handle/FPGA.h"

using namespace ::HMF::Coordinate;

namespace sthal {

void OnlyNeuronNoResetNoFGConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(getLogger(), "Skipping reset and init of FPGA in OnlyNeuronNoResetNoFGConfigurator");
	config_dnc_link(f, fpga);
	LOG4CXX_INFO(
		getLogger(), "finished configuring DNC link of FPGA " << f->coordinate() << " in "
															  << t.get_ms() << "ms");
}

void OnlyNeuronNoResetNoFGConfigurator::config(
	fpga_handle_t const&, hicann_handle_t const& h, hicann_data_t const& hicann)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(
		getLogger(),
		"Partial configuration of " << h->coordinate() << " in OnlyNeuronNoResetNoFGConfigurator");

	config_neuron_config(h, hicann);
	config_neuron_quads(h, hicann);
	config_analog_readout(h, hicann);
}

} // end namespace sthal
