#include "sthal/FGStimulus.h"

using namespace ::halco::hicann::v2;

namespace sthal {

FGStimulus::FGStimulus()
{
}

FGStimulus::FGStimulus(const ::HMF::HICANN::FGStimulus & other) :
	::HMF::HICANN::FGStimulus(other),
	mPulselength(15)
{
}

FGStimulus & FGStimulus::operator=(const ::HMF::HICANN::FGStimulus & other)
{
	if (&other != this)
	{
		static_cast< ::HMF::HICANN::FGStimulus & >(*this) = other;
		mPulselength = 15;
	}
	return *this;
}

bool operator== (FGStimulus const& a, FGStimulus const& b)
{
	return static_cast< const ::HMF::HICANN::FGStimulus & >(a) ==
					static_cast< const ::HMF::HICANN::FGStimulus & >(b)
					&& a.mPulselength == b.mPulselength;
}

bool operator!= (FGStimulus const& a, FGStimulus const& b)
{
	return !(a==b);
}

uint8_t FGStimulus::getPulselength() const
{
       return mPulselength;
}

void FGStimulus::setPulselength(uint8_t const pl)
{
       mPulselength = pl;
}

std::ostream& operator<<(std::ostream& os, FGStimulus const& f)
{
       os << static_cast< const ::HMF::HICANN::FGStimulus & >(f)
          << "  pulselength: " << static_cast<int>(f.getPulselength()) << "\n";
	   return os;
}

} // end namespace sthal
