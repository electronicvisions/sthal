#include "sthal/DNC.h"

#include "hal/Coordinate/iter_all.h"

namespace sthal {

std::vector<DNC::hicann_coord> DNC::getAllocatedHicanns() const
{
	std::vector<hicann_coord> result;
	for (auto hicann : ::HMF::Coordinate::iter_all<hicann_coord>())
	{
		if (mHICANNs[hicann.id()])
			result.push_back(hicann);
	}
	return result;
}

std::ostream& operator<<(std::ostream& out, DNC const& obj)
{
	out << "DNC:" << std::endl;
	for (auto h : obj.mHICANNs) {
		out << "   " << h << std::endl;
	}
	return out;
}

} // end namespace sthal
