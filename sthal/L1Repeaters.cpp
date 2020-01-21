#include <algorithm>
#include <iterator>

#include "L1Repeaters.h"
#include "halco/common/iter_all.h"

namespace sthal {

L1Repeaters::L1Repeaters()
{}

void L1Repeaters::clearReapeater()
{
	std::fill(mHorizontalRepeater.begin(), mHorizontalRepeater.end(),
			  horizontal_type());
	std::fill(mVerticalRepeater.begin(), mVerticalRepeater.end(),
			  vertical_type());
	std::fill(mBlocks.begin(), mBlocks.end(), block_type());
}

::HMF::HICANN::VerticalRepeater L1Repeaters::getRepeater(::halco::hicann::v2::VRepeaterOnHICANN c) const
{
	return mVerticalRepeater[c.toVLineOnHICANN()];
}

::HMF::HICANN::HorizontalRepeater L1Repeaters::getRepeater(::halco::hicann::v2::HRepeaterOnHICANN c) const
{
	return mHorizontalRepeater[c.toHLineOnHICANN()];
}

::HMF::HICANN::RepeaterBlock L1Repeaters::getRepeaterBlock(::halco::hicann::v2::RepeaterBlockOnHICANN block) const
{
	return mBlocks[block.toEnum()];
}

void L1Repeaters::setRepeater(::halco::hicann::v2::VRepeaterOnHICANN c, ::HMF::HICANN::VerticalRepeater const& r)
{
	mVerticalRepeater[c.toVLineOnHICANN()] = r;
}

void L1Repeaters::setRepeater(::halco::hicann::v2::HRepeaterOnHICANN c, ::HMF::HICANN::HorizontalRepeater const& r)
{
	mHorizontalRepeater[c.toHLineOnHICANN()] = r;
}

void L1Repeaters::setRepeaterBlock(::halco::hicann::v2::RepeaterBlockOnHICANN block, ::HMF::HICANN::RepeaterBlock const& r)
{
	mBlocks[block.toEnum()] = r;
}

void L1Repeaters::enable_dllreset()
{
	for (auto& rb : mBlocks) {
		rb.dllresetb = false;
	}
}

void L1Repeaters::disable_dllreset()
{
	for (auto& rb : mBlocks) {
		rb.dllresetb = true;
	}
}

bool L1Repeaters::is_dllreset_disabled() const
{
	return std::all_of(std::cbegin(mBlocks),
	                   std::cend(mBlocks),
	                   [](auto const& rb){return rb.dllresetb;});
}

void L1Repeaters::enable_drvreset()
{
	for (auto& rb : mBlocks) {
		rb.drvresetb = false;
	}
}

void L1Repeaters::disable_drvreset()
{
	for (auto& rb : mBlocks) {
		rb.drvresetb = true;
	}
}

bool L1Repeaters::is_drvreset_disabled() const
{
	return std::all_of(std::cbegin(mBlocks),
	                   std::cend(mBlocks),
	                   [](auto const& rb){return rb.drvresetb;});
}

bool L1Repeaters::operator==(const L1Repeaters & other) const
{
	return mVerticalRepeater   == other.mVerticalRepeater   &&
		   mHorizontalRepeater == other.mHorizontalRepeater &&
		   mBlocks             == other.mBlocks;
}

bool L1Repeaters::operator!=(const L1Repeaters & other) const
{
	return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, L1Repeaters const& a)
{
	os << "active HorizontalRepeaters: " << std::endl;
	for (auto const& r : a.mHorizontalRepeater) {
		if (r.getMode() != HMF::HICANN::HorizontalRepeater::IDLE)
			os << "\t" << r << std::endl;
	}

	os << "active VerticalRepeaters: " << std::endl;
	for (auto const& r : a.mVerticalRepeater) {
		if (r.getMode() != HMF::HICANN::VerticalRepeater::IDLE)
			os << "\t" << r << std::endl;
	}

	os << "RepeaterBlocks: " << std::endl;
	for (auto b_c : ::halco::common::iter_all<L1Repeaters::block_coordinate>()) {
		os << b_c << " " << a.mBlocks[b_c.toEnum()] << std::endl;
	}

	return os;
}

size_t L1Repeaters::count_repeaters(
    ::halco::hicann::v2::RepeaterBlockOnHICANN rb,
    ::halco::hicann::v2::TestPortOnRepeaterBlock tp,
    ::HMF::HICANN::Repeater::Mode mode) const
{
	size_t active = 0;

	for (auto r_c : ::halco::common::iter_all<L1Repeaters::horizontal_coordinate>()) {
		auto const& r = this->getRepeater(r_c);
		if (!r_c.isSending() && r.getMode() == mode && r_c.toRepeaterBlockOnHICANN() == rb &&
		    r_c.toTestPortOnRepeaterBlock() == tp) {
			active += 1;
		}
	}

	for (auto r_c : ::halco::common::iter_all<L1Repeaters::vertical_coordinate>()) {
		auto const& r = this->getRepeater(r_c);
		if (r.getMode() == mode && r_c.toRepeaterBlockOnHICANN() == rb &&
		    r_c.toTestPortOnRepeaterBlock() == tp) {
			active += 1;
		}
	}

	return active;
}

bool L1Repeaters::check_testports(std::ostream& errors) const
{
	using namespace ::halco::hicann::v2;
	using namespace ::halco::common;
	using namespace ::HMF::HICANN;

	bool ok = true;

	for (auto rb : iter_all<RepeaterBlockOnHICANN>()) {
		for (auto tp : iter_all<TestPortOnRepeaterBlock>()) {
			auto const active_outputs = count_repeaters(rb, tp, Repeater::OUTPUT);
			if (active_outputs > 1) {
				ok = false;
				errors << "Warning: " << active_outputs << " > 1 test outputs enabled on " << rb
				       << "/" << tp;
			}
			auto const active_inputs = count_repeaters(rb, tp, Repeater::INPUT);
			if (active_inputs > 1) {
				ok = false;
				errors << "Warning: " << active_inputs << " > 1 test inputs enabled on " << rb
				       << "/" << tp;
			}
		}
	}

	return ok;
}

bool L1Repeaters::check(std::ostream& errors) const
{
	bool ok = true;
	// ok &= check_something_else(errors);
	ok &= check_testports(errors);
	return ok;
}

} // end namespace sthal
