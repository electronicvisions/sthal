#include "sthal/MagicHardwareDatabase.h"

#include "sthal/YAMLHardwareDatabase.h"
#include "sthal/Settings.h"
#include "hal/Coordinate/HMFGeometry.h"

using namespace HMF::Coordinate;

namespace sthal {

MagicHardwareDatabase::MagicHardwareDatabase()
{
	std::string path = Settings::get().yaml_hardware_database_path;
	boost::shared_ptr<YAMLHardwareDatabase> database(
		new YAMLHardwareDatabase());
	database->load(path);
	mDatabase = database;
}

MagicHardwareDatabase::fpga_handle_t
MagicHardwareDatabase::get_fpga_handle(const global_fpga_coord & fpga,
		const std::vector<hicann_coord> & hicanns) const
{
	return mDatabase->get_fpga_handle(fpga, hicanns);
}

bool MagicHardwareDatabase::has_adc_of_hicann(
	const global_hicann_coord& hicann, const analog_coord& analog) const
{
	return mDatabase->has_adc_of_hicann(hicann, analog);
}


ADCConfig
MagicHardwareDatabase::get_adc_of_hicann(const global_hicann_coord & hicann,
		                                 const analog_coord & analog) const
{
	return mDatabase->get_adc_of_hicann(hicann, analog);
}

size_t
MagicHardwareDatabase::get_hicann_version(global_hicann_coord hicann) const
{
	return mDatabase->get_hicann_version(hicann);
}

::HMF::Coordinate::IPv4
MagicHardwareDatabase::get_fpga_ip(const global_fpga_coord & fpga) const
{
	return mDatabase->get_fpga_ip(fpga);
}

::HMF::Coordinate::IPv4
MagicHardwareDatabase::get_fpga_ip(const global_hicann_coord & hicann) const
{
	return mDatabase->get_fpga_ip(hicann);
}

} // namespace sthal
