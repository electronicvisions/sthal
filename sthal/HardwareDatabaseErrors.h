#pragma once

#include <stdexcept>
#include <sstream>

namespace sthal
{

class HardwareDatabaseError : public std::runtime_error
{
public:
	HardwareDatabaseError(const std::string & msg);
	HardwareDatabaseError(const char * const msg);
};

class HardwareDatabaseKeyError : public HardwareDatabaseError
{
public:
	template <typename T>
	HardwareDatabaseKeyError(const char * const msg, T coordinate) :
		HardwareDatabaseError(format(msg, coordinate))
	{}

private:
	template <typename T>
	static std::string format(const char * const msg, T coordinate)
	{
		std::stringstream tmp;
		tmp << std::string(msg) << " " << coordinate;
		return tmp.str();
	}
};

}
