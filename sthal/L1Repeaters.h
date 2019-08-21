#pragma once

#include "hal/HICANNContainer.h"
#include "hal/Coordinate/HMFGeometry.h"

#include "sthal/macros.h"

namespace sthal {

struct L1Repeaters
{
public:
	L1Repeaters();

	// TODO: add repeater control which manages all repeaters and provides
	// convinient functions
	// void get_horizontal...
	// void get_sending
	// void get_vertical

	typedef ::HMF::HICANN::HorizontalRepeater horizontal_type;
	typedef ::HMF::HICANN::VerticalRepeater   vertical_type;
	typedef ::HMF::HICANN::RepeaterBlock      block_type;

	typedef ::HMF::Coordinate::HRepeaterOnHICANN       horizontal_coordinate;
	typedef ::HMF::Coordinate::VRepeaterOnHICANN       vertical_coordinate;
	typedef ::HMF::Coordinate::RepeaterBlockOnHICANN   block_coordinate;

	STHAL_ARRAY_OPERATOR(horizontal_type, horizontal_coordinate,
			return mHorizontalRepeater[ii.toHLineOnHICANN()];)
	STHAL_ARRAY_OPERATOR(vertical_type, vertical_coordinate,
			return mVerticalRepeater[ii.toVLineOnHICANN()];)
	STHAL_ARRAY_OPERATOR(block_type, block_coordinate, return mBlocks[ii.id()];)

	::HMF::HICANN::VerticalRepeater   getRepeater(::HMF::Coordinate::VRepeaterOnHICANN) const;
	::HMF::HICANN::HorizontalRepeater getRepeater(::HMF::Coordinate::HRepeaterOnHICANN) const;
	::HMF::HICANN::RepeaterBlock      getRepeaterBlock(::HMF::Coordinate::RepeaterBlockOnHICANN) const;

	/// Clears all repeater settings
	void clearReapeater();

	void setRepeater(::HMF::Coordinate::VRepeaterOnHICANN, ::HMF::HICANN::VerticalRepeater const&);
	void setRepeater(::HMF::Coordinate::HRepeaterOnHICANN, ::HMF::HICANN::HorizontalRepeater const&);
	void setRepeaterBlock(::HMF::Coordinate::RepeaterBlockOnHICANN, ::HMF::HICANN::RepeaterBlock const&);

	// TODO: add iterator for all horizontal repeaters (including sending repeaters)

	void enable_dllreset();
	void disable_dllreset();
	bool is_dllreset_disabled() const;

	void enable_drvreset();
	void disable_drvreset();
	bool is_drvreset_disabled() const;

	bool operator==(const L1Repeaters & other) const;
	bool operator!=(const L1Repeaters & other) const;
	friend std::ostream& operator<<(std::ostream& os, L1Repeaters const& a);

	std::array<horizontal_type, horizontal_coordinate::enum_type::size> mHorizontalRepeater;
	std::array<vertical_type, vertical_coordinate::enum_type::size>     mVerticalRepeater;
	std::array<block_type, block_coordinate::enum_type::size>           mBlocks;

	// return the number of repeaters that are in mode, assigned to testport tp on repeaterblock rb
	size_t count_repeaters(
	    ::HMF::Coordinate::RepeaterBlockOnHICANN rb,
	    ::HMF::Coordinate::TestPortOnRepeaterBlock tp,
	    ::HMF::HICANN::Repeater::Mode mode) const;

	// return false if an erroneous configuration w.r.t. to testport is detected
	bool check_testports(std::ostream & errors) const;

	// return false if an erroneous configuration is detected
	bool check(std::ostream & errors) const;

private:
	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, unsigned int const)
	{
		ar & boost::serialization::make_nvp("vertical_repeater", mVerticalRepeater)
			& boost::serialization::make_nvp("horizontal_repeater", mHorizontalRepeater)
			& boost::serialization::make_nvp("blocks", mBlocks);
	}
};

} // end namespace sthal

#include "sthal/macros_undef.h"
