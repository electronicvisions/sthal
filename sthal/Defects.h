#pragma once

#include <string>

#include "redman/backend/Backend.h"

namespace sthal {

/// Loads the redman backend using the options given to sthal::Settings
boost::shared_ptr< ::redman::backend::Backend> load_defects_backend();
}
