#pragma once

#include "sthal/NoFGConfigurator.h"

namespace sthal {

/// HICANN Configurator that does not program the floating gates
/// Required for backward compatibility in pyNN.hardware.brainscales
class DontProgramFloatingGatesHICANNConfigurator : public NoFGConfigurator
{
};

} // end namespace sthal
