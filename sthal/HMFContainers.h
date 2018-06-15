#pragma once

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/nvp.hpp>

#include "hal/DNCContainer.h"
#include "hal/FPGAContainer.h"

namespace HMF {

// DNC
struct DNCConfig {
	DNCConfig(){}
	::HMF::DNC::GbitReticle gbit_reticle;
	::HMF::DNC::Loopback loopback;
	bool operator==(const DNCConfig & other) const;
};

// FPGA
struct SpinnIFConfig {
	SpinnIFConfig(){}

	// Spinnaker Settings
    uint16_t receive_port;
    size_t upsample_count;
    size_t downsample_count;
	::HMF::FPGA::SpinnSenderConfig sender_cfg;
	::HMF::FPGA::SpinnAddressConfig addr_cfg;

	::HMF::FPGA::SpinnRoutingTable routing_table;
	bool operator==(const SpinnIFConfig & other) const;
};

struct FPGAConfig {
	FPGAConfig(){}
	// currently only holding SpinnIF config
	SpinnIFConfig spinn_if_cfg;
	bool operator==(const FPGAConfig& other) const {
		return spinn_if_cfg == other.spinn_if_cfg;
	}
};

} // namespace HMF


// boost::serialization stuff
namespace boost {
namespace serialization {

template<typename Archiver>
void serialize(Archiver & ar, HMF::DNCConfig& s, unsigned int const)
{
	using namespace boost::serialization;
	ar & make_nvp("gbit_reticle", s.gbit_reticle)
	   & make_nvp("loopback", s.loopback);
}
template<typename Archiver>
void serialize(Archiver & ar, HMF::SpinnIFConfig& s, unsigned int const)
{
	using namespace boost::serialization;
	ar & make_nvp("receive_port", s.receive_port)
	   & make_nvp("upsample_count", s.upsample_count)
	   & make_nvp("downsample_count", s.downsample_count)
	   & make_nvp("sender_cfg", s.sender_cfg)
	   & make_nvp("addr_cfg", s.addr_cfg)
	   & make_nvp("routing_table", s.routing_table);
}

template<typename Archiver>
void serialize(Archiver & ar, HMF::FPGAConfig& s, unsigned int const)
{
	using namespace boost::serialization;
	ar & make_nvp("spinn_if_cfg", s.spinn_if_cfg);
}

} // namespace serialization
} // namespace boost
