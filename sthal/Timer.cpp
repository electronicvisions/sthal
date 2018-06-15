#include "Timer.h"

using namespace std::chrono;

namespace {

log4cxx::LoggerPtr make_logger() {
	log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("Timer");
	// Do not inherit the appenders of ancestors (i.e. root logger).
	logger->setAdditivity(false);
	return logger;
}

} // namespace

namespace sthal {

double Timer::get_s() const
{
	return duration_cast<duration<double> >(steady_clock::now() - m_start).count();
}

double Timer::get_ms() const
{
	return duration_cast<duration<double, std::milli> >(steady_clock::now() - m_start).count();
}

double Timer::get_us() const
{
	return duration_cast<duration<double, std::micro> >(steady_clock::now() - m_start).count();
}

log4cxx::LoggerPtr Timer::get_logger() {
	static log4cxx::LoggerPtr logger = make_logger();
	return logger;
}

}// end namespace sthal
