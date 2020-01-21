#pragma once

#include "halco/hicann/v2/external.h"

namespace HMF {

// fwd decl
namespace Handle {
struct FPGA;
}

struct SpinnIFConfig;
struct DNCConfig;

namespace FPGA {
void config(const Handle::FPGA& f, const SpinnIFConfig & cfg);
} // namespace FPGA

namespace DNC{
void config(halco::hicann::v2::DNCGlobal const& dnc, const DNCConfig& cfg);
} // namespace DNC

} // HMF

