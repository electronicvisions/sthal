#pragma once

#include <chrono>

#include <log4cxx/logger.h>

namespace sthal {

class Timer
{
public:
	/**
	 * @brief Creates a Timer object for manual measurement of durations.
	 */
	Timer() : m_name(nullptr), m_start(std::chrono::steady_clock::now())
	{
	}

	/**
	 * @brief Creates a Timer object associated with the specified function, that will
	 *        produce output to a special logger instance on destruction.
	 * This variant can be used to provide performance metrics by creating it as an RAII
	 * object early on in the corresponding function:
	 * \code
	 *   auto perflog = Timer::from_literal_string(__PRETTY_FUNCTION__);
	 * \endcode
	 */
	static Timer from_literal_string(char const* name)
	{
		return Timer{name};
	}

	~Timer() {
		using namespace std::chrono;
		if (m_name != nullptr) {
			LOG4CXX_TRACE(
				get_logger(), "\t" << m_name
				<< "\t" << duration_cast<nanoseconds>(m_start.time_since_epoch()).count()
				<< "\t" << duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
		}
	}

	double get_s() const;
	double get_ms() const;
	double get_us() const;

private:
	Timer(char const* name) : m_name(name), m_start(std::chrono::steady_clock::now())
	{
	}

	static log4cxx::LoggerPtr get_logger();
	char const* m_name;
	std::chrono::steady_clock::time_point m_start;
};

}// end sthal namespace
