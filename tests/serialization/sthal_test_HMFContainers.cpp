#include <gtest/gtest.h>

#include <sstream>
#include <fstream>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>

#include "sthal/HICANN.h"
#include "sthal/Wafer.h"
#include "sthal/HMFContainers.h"

using namespace std;

template<typename T>
class HMFContainerTest: public ::testing::Test
{};

namespace HMF {

typedef ::testing::Types<
//	DNCConfig,
	SpinnIFConfig,
	FPGAConfig,
	sthal::FloatingGates,
	sthal::AnalogOutput,
	sthal::L1Repeaters,
	sthal::SynapseArray,
	sthal::Neurons,
	sthal::Layer1,
	sthal::SynapseSwitches,
	sthal::CrossbarSwitches,
	sthal::ADCChannel,
	sthal::FPGAShared,
	sthal::HICANN
> ContainerTypes;
TYPED_TEST_SUITE(HMFContainerTest, ContainerTypes);

TYPED_TEST(HMFContainerTest, HasSerialization) {
	TypeParam obj, obj2;

	std::stringstream stream;
	{
		boost::archive::xml_oarchive oa(stream);
		oa << boost::serialization::make_nvp("obj", obj);
	}
	stream.flush();
	{
		boost::archive::xml_iarchive ia(stream);
		ia >> boost::serialization::make_nvp("obj", obj2);
	}

	ASSERT_EQ(obj2, obj);
}


} // namespace HMF
