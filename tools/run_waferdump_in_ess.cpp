#include <boost/program_options.hpp>

#include "logging_ctrl.h"

#include "sthal/Wafer.h"
#include "sthal/ESSHardwareDatabase.h"
#include "sthal/ESSRunner.h"
#include "sthal/HICANNConfigurator.h"

using namespace sthal;
namespace po = boost::program_options;

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("main");

int main(int argc, char** argv)
{
	logger_default_config(log4cxx::Level::getInfo());
	logger->setLevel(log4cxx::Level::getInfo());

	std::string ess_tempdir;
	std::string infile;
	double ess_runtime;

	po::options_description desc("Allowed options");
	desc.add_options()
		("help", "produce help message")
		("tmp", po::value<std::string>(&ess_tempdir),
		 "ESS temporay folder")
		("dump", po::value<std::string>(&infile)->required(),
		 "dump of sthal container (created with Wafer::dump(...))")
		("runtime", po::value<double>(&ess_runtime)->required(),
		 "Runtime of the simulation")
	;

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
	po::notify(vm);
	if (vm.count("help"))
	{
		std::cout << std::endl << desc << std::endl;
		return 0;
	}

	Wafer wafer;
	wafer.load(infile.c_str());

	ESSHardwareDatabase ess_db(wafer.index(), ess_tempdir);
	ESSRunner ess_runner(ess_runtime);
	HICANNConfigurator ess_configurator;

	wafer.connect(ess_db);
	wafer.configure(ess_configurator);
	wafer.start(ess_runner);
}
