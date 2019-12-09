#include "sthal/DNC.h"

#include <boost/serialization/array.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include "sthal/HICANN.h"

#include "hal/Coordinate/iter_all.h"

namespace sthal {

std::vector<DNC::hicann_coord> DNC::getAllocatedHicanns() const
{
	std::vector<hicann_coord> result;
	for (auto hicann : ::HMF::Coordinate::iter_all<hicann_coord>())
	{
		if (mHICANNs[hicann.toEnum()])
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

bool operator==(DNC const& a, DNC const& b)
{
	for (size_t i = 0; i < a.mHICANNs.size(); i++) {
		if (static_cast<bool>(a.mHICANNs[i]) != static_cast<bool>(b.mHICANNs[i])) {
			return false;
		} else if (static_cast<bool>(a.mHICANNs[i]) && ((*a.mHICANNs[i]) != (*b.mHICANNs[i]))) {
			return false;
		}
	}

	return true;
}

bool operator!=(DNC const& a, DNC const& b)
{
	return !(a == b);
}

template<typename Archiver>
void DNC::serialize(Archiver & ar, unsigned int const)
{
	using boost::serialization::make_nvp;
	ar & make_nvp("hicanns", mHICANNs);
}

} // end namespace sthal

BOOST_CLASS_EXPORT_IMPLEMENT(sthal::DNC)

#include "boost/serialization/serialization_helper.tcc"
EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(sthal::DNC)
