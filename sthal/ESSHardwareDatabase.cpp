#include "sthal/ESSHardwareDatabase.h"

#include <boost/make_shared.hpp>
#include <log4cxx/logger.h>

#include "sthal/HardwareDatabaseErrors.h"
#include "sthal/ADCEssHandleFactory.h"

#include "hal/Handle/FPGAEss.h"
#include "hal/Handle/ADCEss.h"

using namespace HMF::Coordinate;

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("sthal.ESSHardwareDatabase");

namespace sthal {

ESSHardwareDatabase::ESSHardwareDatabase(HMF::Coordinate::Wafer wafer, std::string const& path)
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

ESSHardwareDatabase::fpga_handle_t
ESSHardwareDatabase::get_fpga_handle(const global_fpga_coord & fpga,
		const std::vector<hicann_coord> & hicanns) const
{
	return fpga_handle_t(new ::HMF::Handle::FPGAEss(fpga, mESS, hicanns));
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
    HMF::Coordinate::DNCOnWafer dnc = hicann.toDNCGlobal();
    HMF::ADC::USBSerial adc_coord = HMF::Handle::ADCEss::getVirtualADCCoordinate(dnc,analog);

    return ADCConfig{
		adc_coord,
        ::HMF::Coordinate::ChannelOnADC(0),
		::HMF::Coordinate::TriggerOnADC(0),
		boost::make_shared<ADCEssHandleFactory>(mESS),
		ADCConfig::CalibrationMode::ESS_CALIBRATION
	};
}

::HMF::Coordinate::IPv4
ESSHardwareDatabase::get_fpga_ip(global_fpga_coord const&) const
{
	return ::HMF::Coordinate::IPv4();
}

boost::shared_ptr<HardwareDatabase> ESSHardwareDatabase::clone() const
{
	return boost::shared_ptr<HardwareDatabase>(new ESSHardwareDatabase(*this));
}

} // sthal
