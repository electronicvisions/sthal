#pragma once

#include "hal/HICANNContainer.h"
#include "sthal/AnalogRecorder.h"
#include "sthal/FGConfig.h"
#include "sthal/ParallelHICANNv4Configurator.h"

namespace sthal {
/*
 * The HICANNv4Configurator is used for sequential configuration, i.e configure one HICANN
 * at the time, using the v4-specific floating gate programming scheme implemented in the
 * Parallelv4Configurator. In addition, the Parallelv4Configurator adds configuration stages
 * and user defined sleeps after each stage. When both, v4-FG-programming and configuration stages
 * (with sleeps), should be used, the HICANNv4Configurator can be used instead of the
 * HICANNConfigurator for sequential programming.
 */
class HICANNv4Configurator : public ParallelHICANNv4Configurator
{
};

} // end namespace sthal
