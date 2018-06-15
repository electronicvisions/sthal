#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/ref.hpp>

#include "sthal/HICANNConfigurator.h"
#include "sthal/ParallelHICANNNoFGConfigurator.h"

namespace sthal {

/// Parallel HICANN Configurator that does not program the floating gates nor reset the connection to the FPGA
class ParallelHICANNNoResetNoFGConfigurator : public ParallelHICANNNoFGConfigurator
{
public:
	void config_fpga(fpga_handle_t const& f, fpga_t const& fg) PYPP_OVERRIDE;
	void prime_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	void start_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;

	static log4cxx::LoggerPtr getLogger();
	static log4cxx::LoggerPtr getTimeLogger();
};

} // end namespace sthal
