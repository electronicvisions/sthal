#include "sthal_exception.h"

namespace sthal {
	not_found::not_found(const std::string & what_arg) :
		std::runtime_error(what_arg)
	{
	}

	not_found::not_found(const char* what_arg) :
		std::runtime_error(what_arg)
	{
	}

	not_found::~not_found() noexcept
	{
	}
}
