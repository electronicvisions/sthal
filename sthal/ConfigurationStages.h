#pragma once

#include "pywrap/compat/macros.hpp"

namespace sthal {

PYPP_CLASS_ENUM(ConfigurationStage){
	INIT = 0,
	TIMING_UNCRITICAL = 1,
	LOCKING_REPEATER_BLOCKS = 2,
	LOCKING_SYNAPSE_DRIVERS = 3,
	NEURONS = 4,
	N_STAGES};
}
