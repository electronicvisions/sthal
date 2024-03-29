#include "sthal/MagicHardwareDatabase.h"

#include "sthal/YAMLHardwareDatabase.h"
#include "sthal/Settings.h"
#include "halco/hicann/v2/fwd.h"

using namespace halco::hicann::v2;

namespace sthal {

MagicHardwareDatabase::MagicHardwareDatabase()
{
	std::string path = Settings::get().yaml_hardware_database_path;
	boost::shared_ptr<YAMLHardwareDatabase> database(
		new YAMLHardwareDatabase());
	database->load(path);
	mDatabase = database;
}

MagicHardwareDatabase::fpga_handle_t MagicHardwareDatabase::get_fpga_handle(
    const global_fpga_coord& fpga,
    const Wafer::fpga_t& fpga_data,
    std::vector<Wafer::hicann_t> const& hicanns) const
{
	return mDatabase->get_fpga_handle(fpga, fpga_data, hicanns);
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

::halco::hicann::v2::IPv4
MagicHardwareDatabase::get_fpga_ip(const global_fpga_coord & fpga) const
{
	return mDatabase->get_fpga_ip(fpga);
}

::halco::hicann::v2::IPv4
MagicHardwareDatabase::get_fpga_ip(const global_hicann_coord & hicann) const
{
	return mDatabase->get_fpga_ip(hicann);
}

boost::shared_ptr<HardwareDatabase> MagicHardwareDatabase::clone() const
{
	return boost::shared_ptr<HardwareDatabase>(new MagicHardwareDatabase(*this));
}

} // namespace sthal
