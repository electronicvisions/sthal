#pragma once

#include <boost/shared_ptr.hpp>

namespace halco {
namespace hicann {
namespace v2 {
struct Wafer;
}
}
}
namespace HMF {
namespace Coordinate {
using halco::hicann::v2::Wafer;
}
}

namespace redman {
namespace backend {
class Backend;
}
namespace resources {
class Wafer;
}
}

namespace sthal {
/// Loads the redman backend using the options given to sthal::Settings
/// returns empty shared_ptr if both defects_path and defects_host are empty
boost::shared_ptr< ::redman::backend::Backend> load_defects_backend();

/// Loads the redman wafer resources via load_defects_backend
/// returns new wafer resources if backend is not loaded
boost::shared_ptr< ::redman::resources::Wafer> load_resources_wafer(
    halco::hicann::v2::Wafer const& wafer);
} //sthal
