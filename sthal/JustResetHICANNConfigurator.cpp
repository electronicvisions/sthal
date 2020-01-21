#include "sthal/JustResetHICANNConfigurator.h"

#include <log4cxx/logger.h>

#include "hal/backend/HICANNBackend.h"
#include "halco/common/iter_all.h"
#include "sthal/HICANN.h"

using namespace ::halco::hicann::v2;

namespace sthal {

void JustResetHICANNConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fg)
{
	HICANNConfigurator::config_fpga(f, fg);
}

} // end namespace sthal
