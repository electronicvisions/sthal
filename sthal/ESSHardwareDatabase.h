#pragma once

#include "sthal/HardwareDatabase.h"
#include <string>

namespace HMF { namespace Handle {
class Ess;
}}

namespace sthal {

class ESSHardwareDatabase :
	public HardwareDatabase
{
public:
	ESSHardwareDatabase(halco::hicann::v2::Wafer wafer, std::string const& path);
	~ESSHardwareDatabase();

	virtual fpga_handle_t get_fpga_handle(
	    const global_fpga_coord& fpga,
	    const Wafer::fpga_t& fpga_data,
	    const std::vector<Wafer::hicann_t>& hicanns) const PYPP_OVERRIDE;

	virtual bool has_adc_of_hicann(
		const global_hicann_coord& hicann, const analog_coord& analog) const PYPP_OVERRIDE;

	virtual ADCConfig get_adc_of_hicann(
				const global_hicann_coord & hicann,
				const analog_coord & analog) const PYPP_OVERRIDE;

	virtual ::halco::hicann::v2::IPv4
		get_fpga_ip(const global_fpga_coord& fpga) const;

    void freeHandle();

	boost::shared_ptr<HardwareDatabase> clone() const;

private:
	boost::shared_ptr<HMF::Handle::Ess> mESS;
};

} // sthal
