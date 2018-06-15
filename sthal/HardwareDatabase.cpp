#include "sthal/HardwareDatabase.h"

namespace sthal
{

HardwareDatabase::~HardwareDatabase()
{
}

::HMF::Coordinate::IPv4 HardwareDatabase::get_fpga_ip(
				const global_hicann_coord & hicann) const {
	return get_fpga_ip(hicann.toFPGAGlobal());
}

size_t HardwareDatabase::get_hicann_version(global_hicann_coord) const {
	return 0;
}

} // end namespace sthal

