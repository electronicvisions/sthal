#include "sthal/FGConfig.h"

#include "hal/Coordinate/HMFGeometry.h"

using namespace ::HMF::Coordinate;

namespace sthal {

	FGConfig::FGConfig() :
		::HMF::HICANN::FGConfig(),
		writeDown(false)
	{}

	FGConfig::FGConfig(const ::HMF::HICANN::FGConfig & cfg) :
		::HMF::HICANN::FGConfig(cfg),
		writeDown(false)
	{}

	FGConfig::FGConfig(const ::HMF::HICANN::FGConfig & cfg, bool writeDown_) :
		::HMF::HICANN::FGConfig(cfg),
		writeDown(writeDown_)
	{
	}

	bool operator==(FGConfig const& lhs, FGConfig const& rhs)
	{
		typedef ::HMF::HICANN::FGConfig const & base;
		return static_cast<base>(lhs) == static_cast<base>(rhs) &&
			   lhs.writeDown == rhs.writeDown;

	}
	bool operator!=(FGConfig const& lhs, FGConfig const& rhs)
	{
		return !(lhs == rhs);
	}

	std::ostream& operator<<(std::ostream& os, FGConfig const& fc)
	{
		os << static_cast< ::HMF::HICANN::FGConfig const &>(fc)
		   << "  writing direction: " << (fc.writeDown ? "down" : "up");
		return os;
	}

	const bool FGConfig::WRITE_DOWN;
	const bool FGConfig::WRITE_UP;

} // namespace sthal
