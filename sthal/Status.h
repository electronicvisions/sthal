#pragma once

#include "halco/common/typed_array.h"

#include "hal/ADC/USBSerial.h"
#include "halco/hicann/v2/external.h"

namespace sthal {

struct ADCChannel {
	HMF::ADC::USBSerial board_id;
	halco::hicann::v2::ChannelOnADC channel;
	std::string bitfile_version;

	friend bool operator==(const ADCChannel & a, const ADCChannel & b);
private:
	friend class boost::serialization::access;
	template <typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("board_id", board_id)
		   & make_nvp("channel", channel)
		   & make_nvp("bitfile_version", bitfile_version);
	}
};

struct Status
{
	typedef ::halco::hicann::v2::FPGAOnWafer fpga_coord;

	::halco::hicann::v2::Wafer wafer;
	std::vector< ::halco::hicann::v2::HICANNOnWafer > hicanns;

	std::string git_rev_halbe;
	std::string git_rev_hicann_system;
	std::string git_rev_sthal;

	std::array<uint64_t, fpga_coord::size> fpga_id;
	std::array<uint32_t, fpga_coord::size> fpga_rev;

	// adc_channels[dnc_id][analog]
	typedef halco::common::typed_array<ADCChannel, ::halco::hicann::v2::AnalogOnHICANN> dual_channel_t;
	typedef halco::common::typed_array<dual_channel_t, ::halco::hicann::v2::DNCOnWafer> adc_channels_t;
	adc_channels_t adc_channels;

private:
	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("wafer", wafer)
		   & make_nvp("hicanns", hicanns)
		   & make_nvp("git_rev_halbe", git_rev_halbe)
		   & make_nvp("git_rev_hicann_system", git_rev_hicann_system)
		   & make_nvp("git_rev_sthal", git_rev_sthal);
		// When loading version 0 archives with array size != 12 special care is needed
		// saving is handeled by the definition of the archive version
		if (version < 1 && typename Archiver::is_loading()) {
			fpga_id.fill(0);
			fpga_rev.fill(0);
			std::array<uint64_t, 12> tmp_id;
			std::array<uint32_t, 12> tmp_rev;
			ar & make_nvp("fpga_id", tmp_id)
		     & make_nvp("fpga_rev", tmp_rev);
			std::copy(tmp_id.begin(), tmp_id.end(), fpga_id.begin());
			std::copy(tmp_rev.begin(), tmp_rev.end(), fpga_rev.begin());
		} else {
			ar & make_nvp("fpga_id", fpga_id)
			   & make_nvp("fpga_rev", fpga_rev);
		}
		ar & make_nvp("adc_channels", adc_channels);
	}
};

std::ostream & operator<<(std::ostream & out, const Status & st);
std::ostream& operator<<(std::ostream& out, ADCChannel const& obj);

} // end namespace sthal

BOOST_CLASS_VERSION(sthal::Status, (::halco::hicann::v2::FPGAOnWafer::size == 12 ? 0 : 1))
