#include "sthal/ESSConfig.h"

#include <boost/serialization/nvp.hpp>

namespace sthal
{

ESSConfig::ESSConfig() :
    speedup(10000.),
    timestep(0.1),
    enable_weight_distortion(false),
    weight_distortion(0.),
    enable_timed_merger(true),
    enable_spike_debugging(false),
	pulse_statistics_file(""),
	calib_path("")
{}

bool ESSConfig::operator==(ESSConfig const& rhs) const
{
	return (speedup == rhs.speedup) && (timestep == rhs.timestep) &&
	       (enable_weight_distortion == rhs.enable_weight_distortion) &&
	       (weight_distortion == rhs.weight_distortion) &&
	       (enable_timed_merger == rhs.enable_timed_merger) &&
	       (enable_spike_debugging == rhs.enable_spike_debugging) &&
	       (pulse_statistics_file == rhs.pulse_statistics_file) && (calib_path == rhs.calib_path);
}

bool ESSConfig::operator!=(ESSConfig const& rhs) const
{
	return !((*this) == rhs);
}

std::ostream& operator<<(std::ostream& out, ESSConfig const& obj)
{
	out << "ESSConfig(";
	out << "speedup=" << obj.speedup << ", ";
	out << "timestep=" << obj.timestep << ", ";
	out << "enable_weight_distortion=" << obj.enable_weight_distortion << ", ";
	out << "weight_distortion=" << obj.weight_distortion << ", ";
	out << "enable_timed_merger=" << obj.enable_timed_merger << ", ";
	out << "enable_spike_debugging=" << obj.enable_spike_debugging << ", ";
	out << "pulse_statistics_file=" << obj.pulse_statistics_file << ", ";
	out << "calib_path=" << obj.calib_path << ", ";
	out << ")";
	return out;
}

template<typename Archiver>
void ESSConfig::serialize(Archiver& ar, const unsigned int)
{
	using namespace boost::serialization;
	ar & make_nvp("speedup", speedup)
	   & make_nvp("timestep", timestep)
	   & make_nvp("enable_weight_distortion", enable_weight_distortion)
	   & make_nvp("weight_distortion", weight_distortion)
	   & make_nvp("enable_timed_merger", enable_timed_merger)
	   & make_nvp("enable_spike_debugging", enable_spike_debugging)
	   & make_nvp("pulse_statistics_file", pulse_statistics_file)
	   & make_nvp("calib_path", calib_path);
}

} // end namespace sthal

BOOST_CLASS_EXPORT_IMPLEMENT(::sthal::ESSConfig)

#include "boost/serialization/serialization_helper.tcc"
EXPLICIT_INSTANTIATE_BOOST_SERIALIZE(::sthal::ESSConfig)
