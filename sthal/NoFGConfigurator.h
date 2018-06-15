#pragma once

#include "pywrap/compat/macros.hpp"

#include "sthal/HICANNConfigurator.h"

namespace sthal {

/// HICANN Configurator that does not program the floating gates
/// Required for backward compatibility in pyNN.hardware.brainscales
class NoFGConfigurator : public HICANNConfigurator
{
public:
	void config_floating_gates(
		HICANNConfigurator::hicann_handle_t const& h, hicann_data_t const& hicann) PYPP_OVERRIDE;
};

} // end namespace sthal
