#include <gtest/gtest.h>

#include "sthal/Layer1.h"

using namespace halco::hicann::v2;

typedef boost::variant<
	Merger0OnHICANN,
	Merger1OnHICANN,
	Merger2OnHICANN,
	Merger3OnHICANN> merger_variant;

namespace sthal {

TEST(Layer1, VariantFancyness) {
	Layer1 ll;
	Merger0OnHICANN m{5};
	merger_variant v = m;

	ASSERT_EQ(ll[m], ll[v]);

	ll[m].slow = false;
	ASSERT_FALSE(ll[v].slow);

	ll[m].slow = true;
	ASSERT_TRUE(ll[v].slow);

	Layer1 const& l_const = ll;
	HMF::HICANN::Merger const& mmm = l_const[v];
	ASSERT_TRUE(mmm.slow);

	merger_variant const& v_const = v;
	ASSERT_TRUE(ll[v_const].slow);
}

}
