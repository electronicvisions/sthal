#pragma once

#include "hal/HICANN/FGConfig.h"

namespace sthal {

class FGConfig : public ::HMF::HICANN::FGConfig
{
public:
	FGConfig();
	FGConfig(const ::HMF::HICANN::FGConfig & cfg);
	FGConfig(const ::HMF::HICANN::FGConfig & cfg, bool writeDown);

	bool writeDown;

	friend bool operator==(FGConfig const& lhs, FGConfig const& rhs);
	friend bool operator!=(FGConfig const& lhs, FGConfig const& rhs);
	friend std::ostream& operator<<(std::ostream& os, FGConfig const& fc);

	static const bool WRITE_DOWN = true;
	static const bool WRITE_UP = false;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using namespace boost::serialization;
		ar & make_nvp("base", base_object< ::HMF::HICANN::FGConfig >(*this))
		   & make_nvp("writeDown", writeDown);
	}
};

} // namespace sthal
