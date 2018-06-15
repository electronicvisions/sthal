#pragma once

#include "hal/Handle/FPGA.h"

#include "sthal/FPGA.h"
#include "sthal/ExperimentRunner.h"

#include "pythonic/zip.h"

namespace HMF {
namespace Handle {
	struct FPGA;
} // namespace HMF
} // namespace Handle

namespace sthal {

template<typename Func>
void foreach(const fpga_list & fpgas,
		const fpga_handle_list & handles, Func func)
{
	for (auto item : pythonic::zip(fpgas, handles))
	{
		auto & fpga   = std::get<0>(item);
		auto & handle = std::get<1>(item);

		if (fpga && handle)
		{
			func(*fpga, *handle);
		}
	}
}

}
