#pragma once

#include "pywrap/compat/macros.hpp"

#include "hal/Coordinate/HMFGeometry.h"
#include "hal/ADC/Status.h"
#include "hal/ADC/USBSerial.h"
#include "calibtic/HMF/ADC/ADCCalibration.h"

namespace HMF {
namespace ADC {
	class ADCCalibration;
}
namespace Handle {
	struct ADC;
}
}

namespace sthal {

class ADCConfig;
struct ADCHandleFactory;

class AnalogRecorder
{
public:
	typedef ::HMF::ADC::USBSerial adc_coord;
	typedef ::HMF::Coordinate::ChannelOnADC channel_coord;
	typedef ::HMF::Coordinate::TriggerOnADC trigger_coord;
	typedef float time_type;
	typedef float voltage_type;

	AnalogRecorder(const ADCConfig & config);

	// ECM (2016-08-31): If the destructor throws, this will trigger a
	// std::terminate => in C++11, the destructor is implicitly noexcept(true)!
	// Please continue reading my comment in the cpp file... FIXME
	~AnalogRecorder() PYPP_NOEXCEPT(true);

	/// Set the readout time in seconds?
	double getRecordingTime() const;
	void setRecordingTime(double t);

	adc_coord adc() const;
	channel_coord channel() const;
	trigger_coord trigger() const;

	void activateTrigger();
	void activateTrigger(double t);
	void record();
	void record(double t);
	void record_non_blocking();

	bool hasTriggered() const;
	std::string version() const;
	std::string status() const;

	std::vector<uint16_t> traceRaw() const;
	std::vector<voltage_type> trace() const;

	time_type getTimestamp() const;
	double getSampleRate() const;
	std::vector<time_type> getTimestamps() const;

	void freeHandle();

	/// Set the ADC mux to ground, this effectively disconnects the ADC board from
	/// the wafer
	void switchToGND();

	/// Retrieve the ADC calibration for the given Analog recored
	static boost::shared_ptr< ::HMF::ADC::ADCCalibration >
	loadCalibration(const ADCConfig & config);

	/// Construct a hardware analog recorder based on the given coordinates
	/// Convenience function for tests and tools
	static AnalogRecorder getAnalogRecorder(
		::HMF::ADC::USBSerial adc,
		channel_coord channel = channel_coord(),
		trigger_coord trigger = trigger_coord());

	/// Return the ADCCalibration currently used
	boost::shared_ptr< ::HMF::ADC::ADCCalibration > getCalibration() const;

	friend bool operator==(const AnalogRecorder & a, const AnalogRecorder & b);
private:
	::HMF::Handle::ADC & handle() const;

	enum State
	{
		no_data,
		trigger_activated,
		data_recorded
	};

	// TODO use ADCConfig
	adc_coord                               mCoordinate;
	channel_coord                           mChannel;
	trigger_coord                           mTrigger;
	size_t                                  mSamples;
	boost::shared_ptr< ::HMF::ADC::ADCCalibration > mCalibration;
	boost::shared_ptr< ::HMF::Handle::ADC > mADC;

	State mTiggerState;

    //FIXME specify unit!
	double mSampleRate;
	friend std::ostream& operator<<(std::ostream& out, AnalogRecorder const& obj);
};

} // end namespace sthal
