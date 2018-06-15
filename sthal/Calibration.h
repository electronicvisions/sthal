#pragma once

#include <string>

#include "calibtic/backend/Backend.h"

namespace sthal {

/// Loads the calibtic backend using the options given to sthal::Settings
boost::shared_ptr< ::calibtic::backend::Backend >
load_calibration_backend(const std::string & collection);

}
