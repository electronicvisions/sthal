#pragma once

#include "sthal/NOPHICANNConfigurator.h"

namespace sthal {

class DNCLoopbackConfigurator : public NOPHICANNConfigurator
{
public:
	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
};

} // end namespace sthal
