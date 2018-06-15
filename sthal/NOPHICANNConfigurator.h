#pragma once

#include "sthal/HICANNConfigurator.h"

namespace sthal {

// HICANN Configurator that does nothing
class NOPHICANNConfigurator : public HICANNConfigurator
{
public:
	virtual void config_fpga(fpga_handle_t const& f, fpga_t const& fg);
	virtual void config(fpga_handle_t const& f, hicann_handle_t const& h, hicann_data_t const& fg);
};

} // end namespace sthal
