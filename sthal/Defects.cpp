#include "sthal/Defects.h"

#include <stdexcept>
#include <string>

#include "sthal/Settings.h"

#include "redman/backend/Library.h"
#include "redman/backend/Backend.h"
#include "redman/resources/Wafer.h"

namespace sthal {

boost::shared_ptr<redman::backend::Backend> load_defects_backend()
{
	Settings& s = Settings::get();
	if (s.defects_backend.empty() && s.defects_host.empty()) {
		return boost::shared_ptr<redman::backend::Backend>();
	}

	if ((s.defects_backend == "xml")) {
		auto lib = redman::backend::loadLibrary("libredman_xml.so");
		auto backend = redman::backend::loadBackend(lib);
		backend->config("path", s.defects_host);
		backend->init();
		return backend;
	} else {
		std::string err("Unknown defects backend: ");
		err += s.defects_backend;
		throw std::runtime_error(err);
	}
}

boost::shared_ptr<redman::resources::Wafer> load_resources_wafer(
    HMF::Coordinate::Wafer const& wafer)
{
	auto backend = load_defects_backend();
	return backend ? boost::make_shared<redman::resources::Wafer>(
	                     redman::resources::WaferWithBackend(backend, wafer))
	               : boost::make_shared<redman::resources::Wafer>();
}

} // end namespace sthal
