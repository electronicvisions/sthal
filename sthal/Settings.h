#pragma once

#include <map>
#include <string>
#include <vector>

#include "sthal/ConfigurationStages.h"

namespace sthal {

struct Settings
{
public:
	static void init();
	static void load();
	static void save();

	static Settings & get();

	std::string calibtic_backend; //>! backend: xml
	std::string calibtic_host;	  //>! host (or path to folder containing xml files)

	std::string adc_calibtic_collection; //>! collection to use for adc calibration

	std::string yaml_hardware_database_path; //>! full path to the hardware database file

	std::string datadir; //>! path for read-only architecture-independent data,
	                     //>! can be overwritten by NMPM_DATADIR (sthal will be appended)

	struct CrossbarSwitches
	{
		CrossbarSwitches(
			size_t max_per_row = default_max_switches_per_row,
			size_t max_per_column = default_max_switches_per_column);

		size_t max_switches_per_row;
		size_t max_switches_per_column;

#ifndef PYPLUSPLUS
		static constexpr size_t default_max_switches_per_row = 1;
		static constexpr size_t default_max_switches_per_column = 1;
#else
		static const size_t default_max_switches_per_row;
		static const size_t default_max_switches_per_column;
#endif // !PYPLUSPLUS
	};

	CrossbarSwitches crossbar_switches;

	struct SynapseSwitches
	{
		SynapseSwitches(
			size_t max_per_row = default_max_switches_per_row,
			size_t max_per_column_per_side = default_max_switches_per_column_per_side);

		size_t max_switches_per_row;
		size_t max_switches_per_column_per_side;

#ifndef PYPLUSPLUS
		static constexpr size_t default_max_switches_per_row = 1;
		static constexpr size_t default_max_switches_per_column_per_side = 1;
#else
		static const size_t default_max_switches_per_row;
		static const size_t default_max_switches_per_column_per_side;
#endif // !PYPLUSPLUS
	};

	SynapseSwitches synapse_switches;

	struct CfgStages {
		CfgStages();
		std::map<ConfigurationStage, size_t> sleeps; // in ms
		std::vector<ConfigurationStage> order;
	};

	CfgStages configuration_stages;

	bool ignore_hicann_checks;

private:
	Settings();
	~Settings();
};

} // end namespace sthal
