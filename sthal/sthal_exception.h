#pragma once

#include <stdexcept>

namespace sthal {
	/// This error indicates that a search was not successful.
	class not_found : public std::runtime_error
	{
	public:
		explicit not_found(const std::string & what_arg);
		explicit not_found(const char* what_arg);
		~not_found() noexcept;
	};
}
