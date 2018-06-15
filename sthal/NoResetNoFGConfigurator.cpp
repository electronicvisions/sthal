#include "sthal/NoResetNoFGConfigurator.h"

#include <log4cxx/logger.h>

#include "sthal/Timer.h"
#include "hal/Handle/FPGA.h"

using namespace ::HMF::Coordinate;

namespace sthal {

void NoResetNoFGConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(getLogger(), "Skipping reset and init of FPGA in NoResetNoFGConfigurator");
	config_dnc_link(f, fpga);
	LOG4CXX_INFO(
		getLogger(), "finished configuring DNC link of FPGA " << f->coordinate() << " in "
															  << t.get_ms() << "ms");
}

void NoResetNoFGConfigurator::prime_systime_counter(fpga_handle_t const&)
{
	LOG4CXX_INFO(getLogger(), "Skipping priming of systime counter of FPGA in NoResetNoFGConfigurator");
}

void NoResetNoFGConfigurator::start_systime_counter(fpga_handle_t const&)
{
	LOG4CXX_INFO(getLogger(), "Skipping starting of systime counter of FPGA in NoResetNoFGConfigurator");
}

} // end namespace sthal
