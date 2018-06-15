#pragma once

#include "hal/Coordinate/External.h"

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
void config(Coordinate::DNCGlobal const& dnc, const DNCConfig& cfg);
} // namespace DNC

} // HMF

