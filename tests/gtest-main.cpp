#include <gtest/gtest.h>

#include "logger.h"
#include "test/hwtest.h"
#include "test/CommandLineArgs.h"

#include <boost/program_options.hpp>

class HICANNBackendTestEnvironment : public ::testing::Environment {
public:
	explicit HICANNBackendTestEnvironment() = default;

	void set(const CommandLineArgs & conn ) { g_conn = conn; }
};


int main(int argc, char *argv[])
{
	testing::InitGoogleTest(&argc, argv);

	HICANNBackendTestEnvironment * g_env = new HICANNBackendTestEnvironment;
	g_env->set(CommandLineArgs::parse(argc, argv));
	::testing::AddGlobalTestEnvironment(g_env);

	return RUN_ALL_TESTS();
}
