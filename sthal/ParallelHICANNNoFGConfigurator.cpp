#include "sthal/ParallelHICANNNoFGConfigurator.h"

#include "sthal/HICANN.h"
#include "sthal/Timer.h"

#include "hal/Coordinate/iter_all.h"
#include "hal/backend/HICANNBackend.h"

using namespace ::HMF::Coordinate;

namespace sthal {

void ParallelHICANNNoFGConfigurator::config_floating_gates(
	hicann_handles_t const& handles, hicann_datas_t const& hicanns)
{
	if (handles.size() != hicanns.size())
		throw std::runtime_error("the number of handles and data containers has to be equal");
	const size_t n_hicanns = hicanns.size();

	// configure all hicanns
	for (size_t ii = 0; ii != n_hicanns; ++ii) {
		size_t passes = hicanns[ii]->floating_gates.getNoProgrammingPasses();
		if (passes > 0) {
			size_t pass = passes - 1;
			LOG4CXX_DEBUG(getLogger(), "writing FG config for pass " << pass << " of HICANN " << ii);
			const FloatingGates& fg = hicanns[ii]->floating_gates;
			FGConfig cfg = fg.getFGConfig(Enum(pass));
			for (auto block : iter_all<FGBlockOnHICANN>()) {
				::HMF::HICANN::set_fg_config(*handles[ii], block, cfg);
			}
		}
	}

	LOG4CXX_DEBUG(
		getLogger(), "Floating Gate Programming is skipped in ParallelHICANNNoFGConfigurator");
}

} // end namespace sthal
