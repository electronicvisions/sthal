#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/ref.hpp>

#include "sthal/ParallelHICANNv4Configurator.h"

namespace sthal {

/// Parallel HICANN Configurator that does not program the floating gates
class ParallelHICANNNoFGConfigurator : public ParallelHICANNv4Configurator
{
public:
	void config_floating_gates(hicann_handles_t const& handles, hicann_datas_t const& hicanns)
		PYPP_OVERRIDE;
};

} // end namespace sthal
