#include "sthal/ParallelHICANNNoResetNoFGConfigurator.h"

#include <log4cxx/logger.h>
#include <boost/ref.hpp>

#include "pythonic/zip.h"

#include "halco/common/iter_all.h"
#include "hal/Handle/FPGA.h"
#include "hal/Handle/HICANN.h"
#include "hal/Handle/HICANNHw.h"
#include "hal/backend/HICANNBackend.h"

#include "sthal/HICANN.h"
#include "sthal/Timer.h"


using namespace ::halco::hicann::v2;

namespace sthal {

log4cxx::LoggerPtr ParallelHICANNNoResetNoFGConfigurator::getLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.ParallelHICANNNoResetNoFGConfigurator");
	return _logger;
}

log4cxx::LoggerPtr ParallelHICANNNoResetNoFGConfigurator::getTimeLogger()
{
	static log4cxx::LoggerPtr _logger = log4cxx::Logger::getLogger("sthal.ParallelHICANNNoResetNoFGConfigurator.Time");
	return _logger;
}

void ParallelHICANNNoResetNoFGConfigurator::config_fpga(fpga_handle_t const& f, fpga_t const& fpga)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	LOG4CXX_INFO(
		getLogger(), "Skipping reset and init of FPGA in ParallelHICANNNoResetNoFGConfigurator");
	config_dnc_link(f, fpga);
	LOG4CXX_INFO(
		getLogger(), "finished configuring DNC link of FPGA " << f->coordinate() << " in "
		<< t.get_ms() << "ms");
}

void ParallelHICANNNoResetNoFGConfigurator::prime_systime_counter(fpga_handle_t const&)
{
	LOG4CXX_INFO(getLogger(), "Skipping priming of systime counter of FPGA in ParallelHICANNNoResetNoFGConfigurator");
}

void ParallelHICANNNoResetNoFGConfigurator::start_systime_counter(fpga_handle_t const&)
{
	LOG4CXX_INFO(getLogger(), "Skipping starting of systime counter of FPGA in ParallelHICANNNoResetNoFGConfigurator");
}

} // end namespace sthal

