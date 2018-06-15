#pragma once

#include <boost/shared_ptr.hpp>

#include <pywrap/compat/macros.hpp>

#include "hal/Coordinate/HMFGeometry.h"

#include "sthal/ADCConfig.h"

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

	typedef ::HMF::Coordinate::AnalogOnHICANN analog_coord;
	typedef ::HMF::Coordinate::FPGAGlobal     global_fpga_coord;
	typedef ::HMF::Coordinate::HICANNOnWafer  hicann_coord;
	typedef ::HMF::Coordinate::HICANNGlobal   global_hicann_coord;

	virtual ~HardwareDatabase();

	virtual fpga_handle_t get_fpga_handle(
				const global_fpga_coord & fpga,
				const std::vector<hicann_coord> & hicanns) const = 0;

	virtual bool has_adc_of_hicann(
		const global_hicann_coord& hicann, const analog_coord& analog) const = 0;

	virtual ADCConfig get_adc_of_hicann(
				const global_hicann_coord & hicann,
				const analog_coord & analog) const = 0;

	/// Get the major HICANN version. Return 0 if the version is unkown
	virtual size_t get_hicann_version(global_hicann_coord) const;

	// the base class implementation convertes the HICANN coorindate into its
	// parent FPGA coordinate and calls the fpga coordinate-based overload
	virtual ::HMF::Coordinate::IPv4 get_fpga_ip(
				const global_hicann_coord & hicann) const;

	virtual ::HMF::Coordinate::IPv4 get_fpga_ip(
				const global_fpga_coord & fpga) const = 0;
};

}
