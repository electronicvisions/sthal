#pragma once

#include <boost/variant.hpp>

#include "hal/Coordinate/Merger0OnHICANN.h"
#include "hal/Coordinate/Merger1OnHICANN.h"
#include "hal/Coordinate/Merger2OnHICANN.h"
#include "hal/Coordinate/Merger3OnHICANN.h"

#include "hal/HICANNContainer.h"
#include "hal/HICANN/GbitLink.h"
#include "hal/HICANN/MergerTree.h"
#include "hal/HICANN/DNCMergerLine.h"

namespace sthal {

class Layer1
{
public:
	typedef ::HMF::HICANN::BackgroundGenerator   bg_type;
	typedef ::HMF::HICANN::Merger                merger_type;
	typedef ::HMF::HICANN::DNCMerger             dncmerger_type;
	typedef ::HMF::HICANN::GbitLink::Direction   gbitlink_type;

	typedef ::HMF::Coordinate::BackgroundGeneratorOnHICANN bg_coordinate;
	typedef ::HMF::Coordinate::Merger0OnHICANN merger0_coordinate;
	typedef ::HMF::Coordinate::Merger1OnHICANN merger1_coordinate;
	typedef ::HMF::Coordinate::Merger2OnHICANN merger2_coordinate;
	typedef ::HMF::Coordinate::Merger3OnHICANN merger3_coordinate;
	typedef ::HMF::Coordinate::DNCMergerOnHICANN dncmerger_coordinate;
	typedef ::HMF::Coordinate::GbitLinkOnHICANN gbitlink_coordinate;

	bg_type & operator[](const bg_coordinate & bg);
	const bg_type & operator[](const bg_coordinate & bg) const;

	merger_type &      operator[] (merger0_coordinate const& ii);
	merger_type const& operator[] (merger0_coordinate const& ii) const;

	merger_type &      operator[] (merger1_coordinate const& ii);
	merger_type const& operator[] (merger1_coordinate const& ii) const;

	merger_type &      operator[] (merger2_coordinate const& ii);
	merger_type const& operator[] (merger2_coordinate const& ii) const;

	merger_type &      operator[] (merger3_coordinate const& ii);
	merger_type const& operator[] (merger3_coordinate const& ii) const;

	dncmerger_type &      operator[] (dncmerger_coordinate const& ii);
	dncmerger_type const& operator[] (dncmerger_coordinate const& ii) const;

	gbitlink_type &      operator[] (gbitlink_coordinate const& ii);
	gbitlink_type const& operator[] (gbitlink_coordinate const& ii) const;

#ifndef PYPLUSPLUS
	template<typename... Ts>
	merger_type& operator[](boost::variant<Ts...> const& var)
	{
		Layer1 const& l1 = *this;
		return const_cast<merger_type&>(
			boost::apply_visitor(l1_visitor(l1), var));
	}

	template<typename... Ts>
	merger_type const& operator[](boost::variant<Ts...> const& var) const
	{
		return boost::apply_visitor(l1_visitor(*this), var);
	}
#endif // PYPLUSPLUS

	const ::HMF::HICANN::MergerTree & getMergerTree() const
	{ return mMergerTree; }
	void setMergerTree(const ::HMF::HICANN::MergerTree & tree)
	{ mMergerTree = tree; }

	const ::HMF::HICANN::DNCMergerLine & getDNCMergerLine() const
	{ return mDNCMergers; }
	void setDNCMergerLine(const ::HMF::HICANN::DNCMergerLine & mergers)
	{ mDNCMergers = mergers; }

	const ::HMF::HICANN::GbitLink& getGbitLink() const
	{ return mGbitLink; }
	void setGbitLink(const ::HMF::HICANN::GbitLink & gbitlink)
	{ mGbitLink = gbitlink; }

	const ::HMF::HICANN::BackgroundGeneratorArray &
		getBackgroundGeneratorArray() const
	{ return mBackgroundGenerators; }
	void setBackgroundGeneratorArray(
			const ::HMF::HICANN::BackgroundGeneratorArray & array)
	{ mBackgroundGenerators = array; }

	friend bool operator==(const Layer1 & a, const Layer1 & b);
	friend bool operator!=(const Layer1 & a, const Layer1 & b);
	friend std::ostream& operator<< (std::ostream& os, Layer1 const& a);

	::HMF::HICANN::MergerTree mMergerTree;
	::HMF::HICANN::BackgroundGeneratorArray mBackgroundGenerators;
	::HMF::HICANN::DNCMergerLine mDNCMergers;
	::HMF::HICANN::GbitLink mGbitLink;

private:
	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("merger_tree", mMergerTree)
		   & make_nvp("background_generators", mBackgroundGenerators)
		   & make_nvp("dnc_mergers", mDNCMergers)
		   & make_nvp("gbit_link", mGbitLink);
	}

#ifndef PYPLUSPLUS
	class l1_visitor : public boost::static_visitor<merger_type const&>
	{
	public:
		l1_visitor(Layer1 const& l1) : mL1(l1) {}

		template<typename T>
		merger_type const& operator()(T&& t) const
		{
			return mL1[std::forward<T>(t)];
		}

	private:
		Layer1 const& mL1;
	};
#endif // PYPLUSPLUS
};

} // end namespace sthal
