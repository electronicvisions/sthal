#include "HardwareDatabaseErrors.h"

namespace sthal {

HardwareDatabaseError::HardwareDatabaseError(const std::string & msg) :
	std::runtime_error(msg)
{}

HardwareDatabaseError::HardwareDatabaseError(const char * const msg) :
	std::runtime_error(msg)
{}

} // end namespace sthal
