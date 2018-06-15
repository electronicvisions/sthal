#pragma once

#include "pywrap/compat/macros.hpp"

#include "sthal/NoFGConfigurator.h"

namespace sthal {

/// HICANN Configurator that does not program the floating gates nor reset the connection to the FPGA
class NoResetNoFGConfigurator : public NoFGConfigurator
{
public:
	void config_fpga(fpga_handle_t const& f, fpga_t const& fg) PYPP_OVERRIDE;
	void prime_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
	void start_systime_counter(fpga_handle_t const& f) PYPP_OVERRIDE;
};

} // end namespace sthal
