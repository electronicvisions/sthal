#include "sthal/NoFGConfigurator.h"

#include <log4cxx/logger.h>

#include "hal/backend/HICANNBackend.h"
#include "halco/common/iter_all.h"
#include "sthal/HICANN.h"

using namespace ::halco::hicann::v2;
using namespace ::halco::common;

namespace sthal {

void NoFGConfigurator::config_floating_gates(hicann_handle_t const& h, hicann_data_t const& hicann)
{
	const FloatingGates& fg = hicann->floating_gates;
	size_t passes = fg.getNoProgrammingPasses();
	if (passes > 0) {
		size_t pass = passes - 1;
		LOG4CXX_DEBUG(getLogger(), "writing FG config for pass " << pass);
		for (auto block : iter_all<FGBlockOnHICANN>()) {
			::HMF::HICANN::set_fg_config(*h, block, fg.getFGConfig(Enum(pass)));
		}
	}
	LOG4CXX_DEBUG(
		HICANNConfigurator::getLogger(),
		"Floating Gate Programming is skipped in NoFGConfigurator");
}

} // end namespace sthal
