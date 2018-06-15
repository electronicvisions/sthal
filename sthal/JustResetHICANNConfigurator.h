#pragma once

#include "sthal/NOPHICANNConfigurator.h"

namespace sthal {

/// HICANN Configurator that does only trigger FPGA/HICANN reset (and init)
class JustResetHICANNConfigurator : public NOPHICANNConfigurator
{
public:
	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
};

} // end namespace sthal

