#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/serialization.hpp>

#include "sthal/ParallelHICANNv4SmartConfigurator.h"

using namespace boost::archive;
using namespace boost::serialization;


namespace sthal {

TEST(HMFConfig, ConfiguratorSerialization)
{
	ParallelHICANNv4SmartConfigurator cfg, cfg2;

	std::stringstream stream;
	{
		boost::archive::xml_oarchive oa(stream);
		oa << boost::serialization::make_nvp("cfg", cfg);
	}
	stream.flush();
	{
		boost::archive::xml_iarchive ia(stream);
		ia >> boost::serialization::make_nvp("cfg", cfg2);
	}
}

} // namespace HMF
