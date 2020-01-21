#pragma once

#include <boost/shared_ptr.hpp>

#include <pywrap/compat/macros.hpp>

#include "halco/hicann/v2/fwd.h"

#include "sthal/ADCConfig.h"
#include "sthal/Wafer.h"

namespace HMF {
namespace Handle {

class FPGA;

} // end namespace HMF
} // end namespace Handle

namespace sthal
{

class HardwareDatabase
{
public:
	typedef boost::shared_ptr< ::HMF::Handle::FPGA > fpga_handle_t;

	typedef ::halco::hicann::v2::AnalogOnHICANN analog_coord;
	typedef ::halco::hicann::v2::FPGAGlobal     global_fpga_coord;
	typedef ::halco::hicann::v2::HICANNOnWafer  hicann_coord;
	typedef ::halco::hicann::v2::HICANNGlobal   global_hicann_coord;

	virtual ~HardwareDatabase();

	virtual fpga_handle_t get_fpga_handle(
	    const global_fpga_coord& fpga,
	    const Wafer::fpga_t& fpga_data,
	    std::vector<Wafer::hicann_t> const& hicanns) const = 0;

	virtual bool has_adc_of_hicann(
		const global_hicann_coord& hicann, const analog_coord& analog) const = 0;

	virtual ADCConfig get_adc_of_hicann(
				const global_hicann_coord & hicann,
				const analog_coord & analog) const = 0;

	/// Get the major HICANN version. Return 0 if the version is unkown
	virtual size_t get_hicann_version(global_hicann_coord) const;

	// the base class implementation convertes the HICANN coorindate into its
	// parent FPGA coordinate and calls the fpga coordinate-based overload
	virtual ::halco::hicann::v2::IPv4 get_fpga_ip(
				const global_hicann_coord & hicann) const;

	virtual ::halco::hicann::v2::IPv4 get_fpga_ip(
				const global_fpga_coord & fpga) const = 0;

	virtual boost::shared_ptr<HardwareDatabase> clone() const = 0;
};

}
