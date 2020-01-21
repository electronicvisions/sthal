#include "sthal/ESSHardwareDatabase.h"

#include <boost/make_shared.hpp>
#include <log4cxx/logger.h>

#include "sthal/HardwareDatabaseErrors.h"
#include "sthal/ADCEssHandleFactory.h"

#include "hal/Handle/FPGAEss.h"
#include "hal/Handle/ADCEss.h"

using namespace halco::hicann::v2;

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("sthal.ESSHardwareDatabase");

namespace sthal {

ESSHardwareDatabase::ESSHardwareDatabase(halco::hicann::v2::Wafer wafer, std::string const& path)
    : mESS(new HMF::Handle::Ess(wafer, path))
{
	// see halbe/hal/Handle/Ess.h for all the nicities.
	mESS->enableSpikeDebugging(true);
}

ESSHardwareDatabase::~ESSHardwareDatabase()
{
}

void ESSHardwareDatabase::freeHandle()
{
	if (mESS.use_count() == 1)
	{
		LOG4CXX_INFO(logger, "ESS freed");
	}
	mESS.reset();
}

ESSHardwareDatabase::fpga_handle_t ESSHardwareDatabase::get_fpga_handle(
    const global_fpga_coord& fpga,
    const Wafer::fpga_t& /*fpga_data*/,
    const std::vector<Wafer::hicann_t>& hicanns) const
{
	std::vector<hicann_coord> hicann_coords;
	for (auto hicann : hicanns) {
		hicann_coords.push_back(hicann->index());
	}
	return fpga_handle_t(new ::HMF::Handle::FPGAEss(fpga, mESS, hicann_coords));
}

bool ESSHardwareDatabase::has_adc_of_hicann(
	const global_hicann_coord& /*hicann*/, const analog_coord& /*analog*/) const
{
	return true;
}

ADCConfig
ESSHardwareDatabase::get_adc_of_hicann(const global_hicann_coord & hicann,
		                                 const analog_coord & analog) const
{
    //get the ADC coordinate
    halco::hicann::v2::DNCOnWafer dnc = hicann.toDNCGlobal();
    HMF::ADC::USBSerial adc_coord = HMF::Handle::ADCEss::getVirtualADCCoordinate(dnc,analog);

    return ADCConfig{
		adc_coord,
        ::halco::hicann::v2::ChannelOnADC(0),
		::halco::hicann::v2::TriggerOnADC(0),
		boost::make_shared<ADCEssHandleFactory>(mESS),
		ADCConfig::CalibrationMode::ESS_CALIBRATION
	};
}

::halco::hicann::v2::IPv4
ESSHardwareDatabase::get_fpga_ip(global_fpga_coord const&) const
{
	return ::halco::hicann::v2::IPv4();
}

boost::shared_ptr<HardwareDatabase> ESSHardwareDatabase::clone() const
{
	return boost::shared_ptr<HardwareDatabase>(new ESSHardwareDatabase(*this));
}

} // sthal
