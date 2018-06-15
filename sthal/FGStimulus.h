#pragma once

#include "hal/HICANN/FGStimulus.h"

namespace sthal {

class FGStimulus : public ::HMF::HICANN::FGStimulus
{
public:
	FGStimulus();
	FGStimulus(const ::HMF::HICANN::FGStimulus &);
	FGStimulus & operator=(const ::HMF::HICANN::FGStimulus &);

	PYPP_DEFAULT(FGStimulus(FGStimulus const&));
	PYPP_DEFAULT(FGStimulus& operator=(FGStimulus const&));

	friend bool operator== (FGStimulus const& a, FGStimulus const& b);
	friend bool operator!= (FGStimulus const& a, FGStimulus const& b);

	uint8_t getPulselength() const;
	void setPulselength(uint8_t pl);

	friend std::ostream& operator<<(std::ostream& os, FGStimulus const& f);

private:
	rant::integral_range<uint8_t, 15> mPulselength;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using namespace boost::serialization;
		ar & make_nvp("base", base_object< ::HMF::HICANN::FGStimulus >(*this))
		   & make_nvp("pulselenght", mPulselength);
	}
};

} // end namespace sthal
