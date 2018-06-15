#pragma once

#include "pywrap/compat/macros.hpp"

namespace sthal {

PYPP_CLASS_ENUM(ConfigurationStage) {
	TIMING_UNCRITICAL = 0,
	CONFIG_REPEATER = 1,
	LOCK_REPEATER = 2,
	LOCK_SYNAPSEDRIVERS = 3,
	NEURONS = 4,
	N_STAGES
};

}
