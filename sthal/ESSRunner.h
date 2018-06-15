#pragma once

#include <boost/shared_ptr.hpp>
#include <pywrap/compat/array.hpp>
#include "log4cxx/provisionnode.h"
#include "sthal/ExperimentRunner.h"
#include "sthal/ESSConfig.h"

namespace HMF {
namespace Handle {
	class FPGA;
} // Handle
} // HMF

namespace sthal {

class FPGA;

class ESSRunner :
	public ExperimentRunner
{
public:
	ESSRunner(double run_time_in_s, const ESSConfig& cfg = ESSConfig());
	virtual ~ESSRunner();

	virtual void run(const fpga_list & fpgas, const fpga_handle_list & handles);
private:
	ESSConfig mConfig;
};

} // end namespace sthal
