#if __cplusplus >= 201103L
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <memory>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "sthal/FPGA.h"
#include "sthal/Wafer.h"

#include "devnull.h"

using namespace std;
using namespace boost::archive;
using namespace boost::serialization;


namespace sthal {

TEST(HMFConfig, BinarySerialization)
{
	std::unique_ptr<sthal::Wafer> wafer(new Wafer(halco::hicann::v2::Wafer(halco::common::Enum(0))));
	wafer->operator[](halco::hicann::v2::HICANNOnWafer(halco::common::Enum(0)));

	nullbuf      devnull;
	std::ostream st(&devnull);

	binary_oarchive ar(st);
	ar << make_nvp("System", *wafer);
}

} // namespace HMF
#endif
