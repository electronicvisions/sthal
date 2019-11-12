#include "sthal/Settings.h"

#include "hwdb4cpp/hwdb4cpp.h"

using ms = size_t;

namespace sthal {

namespace {
	std::string getenv_or_default(std::string var, std::string default_value) {
		const char * env_value = std::getenv(var.c_str());
		return env_value ? env_value : default_value;
	}
}

constexpr size_t Settings::CrossbarSwitches::default_max_switches_per_row;
constexpr size_t Settings::CrossbarSwitches::default_max_switches_per_column;

constexpr size_t Settings::SynapseSwitches::default_max_switches_per_row;
constexpr size_t Settings::SynapseSwitches::default_max_switches_per_column_per_side;

Settings::CrossbarSwitches::CrossbarSwitches(size_t max_per_row, size_t max_per_column)
	: max_switches_per_row(max_per_row), max_switches_per_column(max_per_column)
{}

Settings::SynapseSwitches::SynapseSwitches(size_t max_per_row, size_t max_per_column_per_side)
	: max_switches_per_row(max_per_row), max_switches_per_column_per_side(max_per_column_per_side)
{}

Settings::CfgStages::CfgStages()
    : sleeps({{ConfigurationStage::TIMING_UNCRITICAL, ms(0)},
              {ConfigurationStage::CONFIG_REPEATER, ms(0)},
              {ConfigurationStage::LOCK_REPEATER, ms(0)},
              {ConfigurationStage::LOCK_SYNAPSEDRIVERS, ms(0)},
              {ConfigurationStage::NEURONS, ms(0)}}),
      order({ConfigurationStage::TIMING_UNCRITICAL,
			 ConfigurationStage::CONFIG_REPEATER,
			 ConfigurationStage::LOCK_REPEATER,
			 ConfigurationStage::LOCK_SYNAPSEDRIVERS,
			 ConfigurationStage::NEURONS}) {}

Settings::Settings() :
	calibtic_backend("xml"),
	calibtic_host("/ley/data/adc-calib"),
	defects_backend(getenv_or_default("STHAL_DEFECTS_BACKEND", "xml")),
	defects_host(getenv_or_default("STHAL_DEFECTS_PATH", "/wang/data/calibration/brainscales/default")),
	adc_calibtic_collection("adc2"),
	yaml_hardware_database_path(
		std::getenv("STHAL_HWDB_PATH") != nullptr
			? std::getenv("STHAL_HWDB_PATH")
			: (std::getenv("STHAL_YAML_HARDWARE_DATABASE_PATH") != nullptr
				? std::getenv("STHAL_YAML_HARDWARE_DATABASE_PATH")
				: "/wang/data/bss-hwdb/db.yaml")),
	datadir(getenv_or_default("NMPM_DATADIR", DATADIR) + "/sthal/"),
	crossbar_switches(),
	synapse_switches(),
	ignore_hicann_checks(false),
	jtag_frequency(
		std::getenv("STHAL_HICANN_JTAG_FREQUENCY") != nullptr
			? std::stoul(std::getenv("STHAL_HICANN_JTAG_FREQUENCY"))
			: 10e6)
{
}

Settings::~Settings()
{
}

Settings & Settings::get()
{
	static Settings instance;
	return instance;
}

} // end namespace sthal
