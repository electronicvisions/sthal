#pragma once

#include "sthal/HardwareDatabase.h"

namespace sthal
{

// Provides IP and ADC board information for single HICANN
class MagicHardwareDatabase : public HardwareDatabase
{
public:
	MagicHardwareDatabase();

	virtual fpga_handle_t get_fpga_handle(
	    const global_fpga_coord& fpga,
	    const Wafer::fpga_t& fpga_data,
	    std::vector<Wafer::hicann_t> const& hicanns) const PYPP_OVERRIDE;

	virtual bool has_adc_of_hicann(
		const global_hicann_coord& hicann, const analog_coord& analog) const PYPP_OVERRIDE;

	virtual ADCConfig get_adc_of_hicann(
				const global_hicann_coord & hicann,
				const analog_coord & analog) const PYPP_OVERRIDE;

	virtual size_t get_hicann_version(global_hicann_coord) const;

	virtual ::halco::hicann::v2::IPv4 get_fpga_ip(
				const global_hicann_coord & hicann) const;

	virtual ::halco::hicann::v2::IPv4 get_fpga_ip(
				const global_fpga_coord & fpga) const;

	boost::shared_ptr<HardwareDatabase> clone() const;

private:
	boost::shared_ptr<HardwareDatabase> mDatabase;
};

}
