#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include "sthal/ESSHardwareDatabase.h"
#include "sthal/MagicHardwareDatabase.h"
#include "sthal/YAMLHardwareDatabase.h"

using namespace HMF::Coordinate;

namespace sthal {

// Just instantiating classes to ensure, that they are not pure virtual

TEST(Databases, MagicHardwareDatabase)
{
	MagicHardwareDatabase db;
}

TEST(Database, YAMLHardwareDatabase)
{
	YAMLHardwareDatabase db;
}

#if defined(HAVE_ESS)
TEST(Databases, ESSHardwareDatabase)
{
	ESSHardwareDatabase db(Wafer(), "/dummy/p4th");
}
#endif
}
