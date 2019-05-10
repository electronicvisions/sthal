#include "sthal/Defects.h"

#include <stdexcept>

#include "sthal/Settings.h"

#include "redman/backend/Library.h"

namespace sthal {

boost::shared_ptr<redman::backend::Backend> load_defects_backend()
{
	Settings& s = Settings::get();

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

} // end namespace sthal
