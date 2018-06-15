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
	ESSHardwareDatabase(HMF::Coordinate::Wafer wafer, std::string const& path);
	~ESSHardwareDatabase();

	virtual fpga_handle_t get_fpga_handle(
				const global_fpga_coord & fpga,
				const std::vector<hicann_coord> & hicanns) const PYPP_OVERRIDE;

	virtual bool has_adc_of_hicann(
		const global_hicann_coord& hicann, const analog_coord& analog) const PYPP_OVERRIDE;

	virtual ADCConfig get_adc_of_hicann(
				const global_hicann_coord & hicann,
				const analog_coord & analog) const PYPP_OVERRIDE;

	virtual ::HMF::Coordinate::IPv4
		get_fpga_ip(const global_fpga_coord& fpga) const;

    void freeHandle();

private:
	boost::shared_ptr<HMF::Handle::Ess> mESS;
};

} // sthal
