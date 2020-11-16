#include "sthal/Layer1.h"
#include "halco/common/iter_all.h"
#include "hate/variant.h"

namespace {
/// (unexposed) helper class to percolate payload through merger tree
class MergerWithPayload
{
public:
	MergerWithPayload() = default;
	MergerWithPayload(
	    HMF::HICANN::Merger::config_t c,
	    sthal::Layer1::merger_payload_vec_t l,
	    sthal::Layer1::merger_payload_vec_t r) :
	    config(c), left(l), right(r)
	{}

	/// retrieve output depending on merger configuration
	sthal::Layer1::merger_payload_vec_t output() const
	{
		switch (config.to_ulong()) {
			case HMF::HICANN::Merger::LEFT_ONLY:
				return left;
				break;
			case HMF::HICANN::Merger::RIGHT_ONLY:
				return right;
				break;
			case HMF::HICANN::Merger::MERGE: {
				sthal::Layer1::merger_payload_vec_t out;
				out.insert(std::end(out), std::begin(left), std::end(left));
				out.insert(std::end(out), std::begin(right), std::end(right));
				return out;
				break;
			}
			default:
				throw std::runtime_error(
				    "unknown merger mode: " + std::to_string(config.to_ulong()));
		}
	}

private:
	HMF::HICANN::Merger::config_t config;
	sthal::Layer1::merger_payload_vec_t left;
	sthal::Layer1::merger_payload_vec_t right;
};
}

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

Layer1::dnc_merger_output_t Layer1::getDNCMergerOutput(
    bool respect_bkg_enable, bool respect_gbitlink_direction) const
{
	halco::common::typed_array<MergerWithPayload, merger0_coordinate> merger0s_with_payload;
	halco::common::typed_array<MergerWithPayload, merger1_coordinate> merger1s_with_payload;
	halco::common::typed_array<MergerWithPayload, merger2_coordinate> merger2s_with_payload;
	halco::common::typed_array<MergerWithPayload, merger3_coordinate> merger3s_with_payload;
	halco::common::typed_array<MergerWithPayload, dncmerger_coordinate> dncmergers_with_payload;

	// dispatch lookup to one of the variables above
	auto my_visitor = hate::overloaded(
	    [&merger0s_with_payload](merger0_coordinate const& m) -> auto& {
		    return merger0s_with_payload[m];
	    },
	    [&merger1s_with_payload](merger1_coordinate const& m) -> auto& {
		    return merger1s_with_payload[m];
	    },
	    [&merger2s_with_payload](merger2_coordinate const& m) -> auto& {
		    return merger2s_with_payload[m];
	    },
	    [&merger3s_with_payload](merger3_coordinate const& m) -> auto& {
		    return merger3s_with_payload[m];
	    },
	    [&dncmergers_with_payload](dncmerger_coordinate const& m) -> auto& {
		    return dncmergers_with_payload[m];
	    });
	typedef std::variant<
	    merger0_coordinate, merger1_coordinate, merger2_coordinate, merger3_coordinate,
	    dncmerger_coordinate>
	    merger_coordinate_t;
	auto merger_with_payload = [&](merger_coordinate_t const& m) -> auto&
	{
		return std::visit(my_visitor, m);
	};

	// incorporate NeuronBlock and (conditionally) BackgroundGenerator
	for (auto m0 : halco::common::iter_all<merger0_coordinate>()) {
		bg_coordinate const bkg_coord(m0.toEnum());
		merger_payload_vec_t const left_payload =
		    ((respect_bkg_enable == false || (*this)[bkg_coord].enable())
		         ? merger_payload_vec_t{{bkg_coord}}
		         : merger_payload_vec_t());
		merger_with_payload(m0) = {(*this)[m0].config, left_payload, {nb_coordinate(m0.toEnum())}};
	}

	// incorporate inner merger stages
	auto incorporate = [&]<typename S /*sending type*/>(auto const& receiving_merger_coordinate)
	{
		merger_with_payload(receiving_merger_coordinate) = {
		    (*this)[receiving_merger_coordinate].config,
		    merger_with_payload(S(receiving_merger_coordinate.toEnum().value() * 2)).output(),
		    merger_with_payload(S(receiving_merger_coordinate.toEnum().value() * 2 + 1)).output()};
	};

	for (auto m1 : halco::common::iter_all<merger1_coordinate>()) {
		incorporate.operator()<merger0_coordinate>(m1);
	}

	for (auto m2 : halco::common::iter_all<merger2_coordinate>()) {
		incorporate.operator()<merger1_coordinate>(m2);
	}

	for (auto m3 : halco::common::iter_all<merger3_coordinate>()) {
		incorporate.operator()<merger2_coordinate>(m3);
	}

	// encode hardware topology of dnc merger stage
	halco::common::typed_array<merger_coordinate_t, dncmerger_coordinate> right_dnc_merger_inputs =
	    {merger0_coordinate(0), merger1_coordinate(0), merger0_coordinate(2),
	     merger3_coordinate(0), merger0_coordinate(4), merger2_coordinate(1),
	     merger1_coordinate(3), merger0_coordinate(7)};

	// incorporate merger and (conditionally) gbitlinks
	for (auto gbitlink : halco::common::iter_all<gbitlink_coordinate>()) {
		dncmerger_coordinate const dncmerger_coord = gbitlink.toDNCMergerOnHICANN();
		merger_payload_vec_t const left_input =
		    ((respect_gbitlink_direction == false ||
		      (*this)[gbitlink] == HMF::HICANN::GbitLink::Direction::TO_HICANN)
		         ? merger_payload_vec_t{{gbitlink}}
		         : merger_payload_vec_t());
		merger_payload_vec_t const right_input =
		    merger_with_payload(right_dnc_merger_inputs[dncmerger_coord]).output();
		merger_with_payload(dncmerger_coord) = {(*this)[dncmerger_coord].config, left_input,
		                                        right_input};
	}

	// collect output
	dnc_merger_output_t dnc_merger_output;
	for (auto dnc_merger : halco::common::iter_all<dncmerger_coordinate>()) {
		dnc_merger_output[dnc_merger] = merger_with_payload(dnc_merger).output();
	}

	return dnc_merger_output;
}

} // end namespace sthal
