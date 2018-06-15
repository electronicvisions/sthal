#include "sthal/Calibration.h"

#include <stdexcept>

#include "sthal/Settings.h"

#include "calibtic/backend/Library.h"

using namespace calibtic::backend;

namespace sthal {

boost::shared_ptr< Backend >
load_calibration_backend(const std::string & /*collection*/)
{
	Settings & s = Settings::get();

	if( (s.calibtic_backend == "xml") )
	{
		auto lib = loadLibrary("libcalibtic_xml.so");
		auto backend = loadBackend(lib);
		backend->config("path", s.calibtic_host);
		backend->init();
		return backend;
	}
	else
	{
		std::string err("Unknown calibtic backend: ");
		err += s.calibtic_backend;
		throw std::runtime_error(err);
	}
}

} // end namespace sthal
