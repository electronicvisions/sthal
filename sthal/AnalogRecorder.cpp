#include "sthal/AnalogRecorder.h"

#include <chrono>
#include <thread>

#include <boost/make_shared.hpp>

#include <log4cxx/logger.h>

#include "sthal/ADCConfig.h"
#include "sthal/ADCHandleFactory.h"
#include "sthal/ADCHwHandleFactory.h"
#include "sthal/Calibration.h"
#include "sthal/Settings.h"
#include "sthal/Timer.h"

#include "hal/Handle/ADC.h"
#include "hal/Handle/ADCHw.h"
#include "hal/backend/ADCBackend.h"

using namespace ::HMF::ADC;

static log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("sthal.AnalogRecorder");

namespace sthal {

AnalogRecorder::AnalogRecorder(const ADCConfig & cfg) :
	mCoordinate(cfg.coord),
	mChannel(cfg.channel),
	mTrigger(cfg.trigger),
	mSamples(0),
	mCalibration(loadCalibration(cfg)),
	mADC(cfg.factory->create(mCoordinate)),
	mTiggerState(no_data)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	mSampleRate = ::HMF::ADC::get_sample_rate(*mADC);
	LOG4CXX_INFO(logger,
		"Created AnalogRecorder for " << mCoordinate <<
		" (" << mChannel << " and sampling rate " << mSampleRate << ")");
}

AnalogRecorder::~AnalogRecorder() noexcept(true)
{
	// ECM (2016-8-31): This is wrong, the destructor is noexcept(true) but
	// freeHandle could throw... however, boost::python::~value_holder is
	// specified as such => so we can't just mark it noexcept(false) here :/.
	freeHandle();
}

bool operator==(const AnalogRecorder & a, const AnalogRecorder & b)
{
	return	a.mCoordinate    == b.mCoordinate
		&&  a.mChannel       == b.mChannel
		&&  a.mTrigger       == b.mTrigger
		&&  a.mSampleRate    == b.mSampleRate
		&&  a.mCalibration   == b.mCalibration;

}

AnalogRecorder::adc_coord  AnalogRecorder::adc() const
{
	return mCoordinate;
}

AnalogRecorder::channel_coord AnalogRecorder::channel() const
{
	return mChannel;
}

AnalogRecorder::trigger_coord AnalogRecorder::trigger() const
{
	return mTrigger;
}

void AnalogRecorder::setRecordingTime(double t)
{
	mSamples = t * mSampleRate;
}

double AnalogRecorder::getRecordingTime() const
{
	return mSamples / mSampleRate;
}

double AnalogRecorder::getSampleRate() const
{
    return mSampleRate;
}

boost::shared_ptr< ::HMF::ADC::ADCCalibration >
AnalogRecorder::loadCalibration(const ADCConfig & cfg)
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	switch (cfg.loadCalibration)
	{
		case ADCConfig::CalibrationMode::LOAD_CALIBRATION:
		{
			Settings & s = Settings::get();
			auto backend = load_calibration_backend(s.adc_calibtic_collection);
			calibtic::MetaData md;
			::HMF::ADC::USBSerial serial(cfg.coord);
			return ::HMF::ADC::loadADCCalibration(backend, md, serial);
		}
		case ADCConfig::CalibrationMode::ESS_CALIBRATION:
			return boost::shared_ptr< ::HMF::ADC::ADCCalibration >(
					new ::HMF::ADC::ADCCalibration(
						::HMF::ADC::ADCCalibration::getESSCalibration()));
		case ADCConfig::CalibrationMode::DEFAULT_CALIBRATION:
			return boost::shared_ptr< ::HMF::ADC::ADCCalibration >(
					new ::HMF::ADC::ADCCalibration(
						::HMF::ADC::ADCCalibration::getDefaultCalibration()));
	}
	throw std::runtime_error("unreachable");
}

AnalogRecorder AnalogRecorder::getAnalogRecorder(
		adc_coord adc, channel_coord channel, trigger_coord trigger)
{
	boost::shared_ptr<ADCHandleFactory> factory{new ADCHwHandleFactory};
	ADCConfig cfg{adc, channel, trigger, factory, ADCConfig::CalibrationMode::LOAD_CALIBRATION};
	return AnalogRecorder(cfg);
}

boost::shared_ptr< ::HMF::ADC::ADCCalibration >
AnalogRecorder::getCalibration() const
{
	return mCalibration;
}

void AnalogRecorder::activateTrigger()
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	::HMF::ADC::Config cfg(mSamples, mChannel, mTrigger);
	::HMF::ADC::config(handle(), cfg);
	::HMF::ADC::prime(handle());
	std::chrono::duration<double> sleep_time(getRecordingTime());
	LOG4CXX_INFO(logger, "AnalogRecorder " << mCoordinate << " (" << mChannel << ") activated trigger to record " << sleep_time.count() << "s");
	mTiggerState = trigger_activated;
}

void AnalogRecorder::activateTrigger(double t)
{
	auto timer = Timer::from_literal_string(__PRETTY_FUNCTION__);
	setRecordingTime(t);
	activateTrigger();
}

void AnalogRecorder::record()
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	::HMF::ADC::Config cfg(mSamples, mChannel, mTrigger);
	::HMF::ADC::config(handle(), cfg);
	::HMF::ADC::trigger_now(handle());
	std::chrono::duration<double> sleep_time(getRecordingTime());
	LOG4CXX_INFO(logger, "AnalogRecorder " << mCoordinate << " (" << mChannel << ") start recording for " << sleep_time.count() << "s");
	std::this_thread::sleep_for(sleep_time);
	mTiggerState = data_recorded;
}

void AnalogRecorder::record_non_blocking()
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	::HMF::ADC::Config cfg(mSamples, mChannel, mTrigger);
	::HMF::ADC::config(handle(), cfg);
	::HMF::ADC::trigger_now(handle());
	LOG4CXX_INFO(logger, "AnalogRecorder " << mCoordinate << " (" << mChannel << ") start recording for");
	mTiggerState = data_recorded;
}

void AnalogRecorder::record(double t)
{
	auto timer = Timer::from_literal_string(__PRETTY_FUNCTION__);
	setRecordingTime(t);
	record();
}

bool AnalogRecorder::hasTriggered() const
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	auto status = ::HMF::ADC::get_status(handle());
	return status.triggered;
}

std::string AnalogRecorder::version() const
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	auto status = ::HMF::ADC::get_status(handle());
	return status.version_string;
}

std::string AnalogRecorder::status() const
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	std::stringstream out;
	out << ::HMF::ADC::get_status(handle());
	return out.str();
}

std::vector<uint16_t> AnalogRecorder::traceRaw() const
{
	switch (mTiggerState) {
		case no_data:
			throw std::runtime_error("No data recorded yet, call record or activateTrigger");
		case trigger_activated:
			if (!::HMF::ADC::get_status(handle()).triggered) {
				throw std::runtime_error("Trigger has been set, but no data has been recorded yet");
			}
		case data_recorded:
			;
	}
	auto ret = ::HMF::ADC::get_trace(handle());

	//check for returned analog sample size
	if (ret.size() < mSamples) {
		LOG4CXX_WARN(logger, "HALbe returned too few ADC samples: " << ret.size() << " < " << mSamples);
		throw std::runtime_error("HALbe returned too few ADC samples.");
	}
	if (ret.size() > mSamples) {
		LOG4CXX_DEBUG(logger, "AnalogRecorder " << mCoordinate << " ("
			<< mChannel << ") recorded " << ret.size()
			<< " samples which is larger than requested " << mSamples
			<< "; discarding rest (This is expected with active ADC compression)");
		ret.resize(mSamples);
	}

	return ret;
}

AnalogRecorder::time_type AnalogRecorder::getTimestamp() const
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	return 1.0/mSampleRate;
}

std::vector<AnalogRecorder::time_type> AnalogRecorder::getTimestamps() const
{
	std::vector<time_type> timestamps(mSamples);
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	for (size_t ii = 0; ii < timestamps.size(); ++ii)
		timestamps[ii] = static_cast<time_type>(ii)/mSampleRate;
	return timestamps;
}

std::vector<AnalogRecorder::voltage_type> AnalogRecorder::trace() const
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	return mCalibration->apply(mChannel, traceRaw());
}

::HMF::Handle::ADC & AnalogRecorder::handle() const
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	if (!mADC)
	{
		throw std::runtime_error("Invalid handle in ADC");
	}
	return *mADC;
}

void AnalogRecorder::freeHandle()
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	if (mADC.use_count() == 1)
	{
		LOG4CXX_INFO(logger, "AnalogRecorder " << mCoordinate << " (" << mChannel << ") free handle");
	}
	mADC.reset();
}

void AnalogRecorder::switchToGND()
{
	auto t = Timer::from_literal_string(__PRETTY_FUNCTION__);
	using ::halco::hicann::v2::ChannelOnADC;
	::HMF::ADC::Config cfg(mSamples, ChannelOnADC::GND, mTrigger);
	::HMF::ADC::config(handle(), cfg);
	LOG4CXX_INFO(logger, "AnalogRecorder " << mCoordinate << " set to GND");
}

std::ostream& operator<<(std::ostream& out, AnalogRecorder const& obj)
{
	out << "AnalogRecorder(";
	out << "Coord=" << obj.mCoordinate << ", ";
	out << "Channel=" << obj.mChannel << ", ";
	out << "Trigger=" << obj.mTrigger << ", ";
	out << "SampleRate=" << obj.mSampleRate << ", ";
	out << "Calibration=" << obj.mCalibration << ")";
	return out;
}
} // end namespace sthal
