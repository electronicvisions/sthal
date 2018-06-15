#include "sthal/NOPHICANNConfigurator.h"

#include <log4cxx/logger.h>

namespace sthal {

void NOPHICANNConfigurator::config(
	fpga_handle_t const&, hicann_handle_t const&, hicann_data_t const&)
{
	LOG4CXX_DEBUG(HICANNConfigurator::getLogger(), "no configuration in NOPHICANNConfigurator");
}

void NOPHICANNConfigurator::config_fpga(fpga_handle_t const&, fpga_t const&)
{
	LOG4CXX_DEBUG(HICANNConfigurator::getLogger(), "no fpga configuration in NOPHICANNConfigurator");
}

} // end namespace sthal
