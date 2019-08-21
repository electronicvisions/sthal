#pragma once

#include "pywrap/compat/macros.hpp"

namespace sthal {

PYPP_CLASS_ENUM(ConfigurationStage){
	TIMING_UNCRITICAL = 0,
	LOCKING_REPEATER_BLOCKS = 1,
	LOCKING_SYNAPSE_DRIVERS = 2,
	NEURONS = 3,
	N_STAGES};
}
