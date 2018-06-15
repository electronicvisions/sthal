#include "sthal/JustResetHICANNConfigurator.h"

#include <log4cxx/logger.h>

#include "hal/backend/HICANNBackend.h"
#include "hal/Coordinate/iter_all.h"
#include "sthal/HICANN.h"

using namespace ::HMF::Coordinate;

namespace sthal {

void JustResetHICANNConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fg)
{
	HICANNConfigurator::config_fpga(f, fg);
}

} // end namespace sthal
