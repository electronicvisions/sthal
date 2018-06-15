#include "sthal/Layer1.h"


namespace sthal {

Layer1::bg_type & Layer1::operator[](const bg_coordinate & bg)
{
	return mBackgroundGenerators[bg];
}

const Layer1::bg_type & Layer1::operator[](const bg_coordinate & bg) const
{
	return mBackgroundGenerators[bg];
}

Layer1::merger_type &         Layer1::operator[] (merger0_coordinate const& ii)
{
	return mMergerTree[ii];
}

Layer1::merger_type const&    Layer1::operator[] (merger0_coordinate const& ii) const
{
	return mMergerTree[ii];
}

Layer1::merger_type &         Layer1::operator[] (merger1_coordinate const& ii)
{
	return mMergerTree[ii];
}

Layer1::merger_type const&    Layer1::operator[] (merger1_coordinate const& ii) const
{
	return mMergerTree[ii];
}

Layer1::merger_type &         Layer1::operator[] (merger2_coordinate const& ii)
{
	return mMergerTree[ii];
}

Layer1::merger_type const&    Layer1::operator[] (merger2_coordinate const& ii) const
{
	return mMergerTree[ii];
}

Layer1::merger_type &         Layer1::operator[] (merger3_coordinate const& ii)
{
	return mMergerTree[ii];
}

Layer1::merger_type const&    Layer1::operator[] (merger3_coordinate const& ii) const
{
	return mMergerTree[ii];
}

Layer1::dncmerger_type &      Layer1::operator[] (dncmerger_coordinate const& ii)
{
	return mDNCMergers[ii];
}

Layer1::dncmerger_type const& Layer1::operator[] (dncmerger_coordinate const& ii) const
{
	return mDNCMergers[ii];
}

Layer1::gbitlink_type &       Layer1::operator[] (gbitlink_coordinate const& ii)
{
	return mGbitLink[ii];
}

Layer1::gbitlink_type const&  Layer1::operator[] (gbitlink_coordinate const& ii) const
{
	return mGbitLink[ii];
}

bool operator==(const Layer1 & a, const Layer1 & b)
{
	return
		   a.mMergerTree            == b.mMergerTree
		&& a.mBackgroundGenerators  == b.mBackgroundGenerators
		&& a.mDNCMergers            == b.mDNCMergers
		&& a.mGbitLink              == b.mGbitLink;
}

bool operator!=(const Layer1 & a, const Layer1 & b)
{
	return !(a == b);
}

std::ostream& operator<< (std::ostream& os, Layer1 const& a)
{
	os << a.mMergerTree;
	os << "BackgroundGenerators:" << std::endl;
	for (auto const& bg : a.mBackgroundGenerators)
		os << "\t" << bg << std::endl;
	os << a.mDNCMergers << std::endl;
	os << a.mGbitLink << std::endl;

	return os;
}

} // end namespace sthal
