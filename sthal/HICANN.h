#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/weak_ptr.hpp>

#ifndef PYPLUSPLUS
#include "hal/Coordinate/typed_array.h"
#endif
#include "sthal/AnalogRecorder.h"
#include "sthal/HICANNData.h"
#include "sthal/Spike.h"
#include "sthal/ADCConfig.h"
#include "sthal/SpeedUp.h"

namespace sthal {

class Wafer;
class FPGA;

// TODO set activate_firing true for all neurons

struct HICANN : public HICANNData {
public:
	typedef ::HMF::HICANN::L1Address            L1Address;

	typedef ::HMF::Coordinate::AnalogOnHICANN   analog_coord;
	typedef ::HMF::Coordinate::GbitLinkOnHICANN dnc_merger_coord;
	typedef ::HMF::Coordinate::HICANNGlobal     hicann_coord;
	typedef ::HMF::Coordinate::HICANNGlobal     index_type;
	typedef ::HMF::Coordinate::NeuronOnHICANN   neuron_coord;
	typedef ::HMF::Coordinate::SynapseDriverOnHICANN synapse_driver_coord;
	typedef ::HMF::Coordinate::SynapseRowOnHICANN    synapse_row_coord;

	// for backward compatibility
	typedef sthal::SpeedUp SpeedUp;


	HICANN();
	HICANN(
	    const hicann_coord& hicann);
	HICANN(
	    const hicann_coord& hicann,
	    const boost::shared_ptr<FPGA> fpga);
	HICANN(
	    const hicann_coord& hicann,
	    const boost::shared_ptr<FPGA> fpga,
	    Wafer* wafer);
	~HICANN();

	hicann_coord const& index() const;

	/// set Config of ADC
	void setADCConfig(const analog_coord & ii, const ADCConfig & adc);

	/// get ADC config
	/// @throws sthal::not_found if no ADC is found
	ADCConfig getADCConfig(const analog_coord & ii);

    /// Clear adc config
    void resetADCConfig();

	/// Set HICANN version, called by Wafer::connect
	void set_version(size_t version);

	/// Get the major HICANN version, throws before connect
	size_t get_version() const;

	/// Get the minor HICANN version, throws before connect
	/// TODO size_t get_minor_version() const;

	// Return new analog recorder with lifetime managment and lock
	AnalogRecorder analogRecorder(const analog_coord & ii);

	void setCurrentStimulus(const neuron_coord & ii, const FGStimulus & stim);
#ifndef PYPLUSPLUS
	FGStimulus & getCurrentStimulus(const neuron_coord & ii);
#endif // PYPLUSPLUS

	std::vector<double> getStimCurrents(const neuron_coord & ii);
	//TODO void setStimCurrents(const neuron_coord & ii, pyublas::vector<double>);
#ifndef PYPLUSPLUS
	//TODO void setStimCurrents(const neuron_coord & ii, std::vector<double>);
#endif // PYPLUSPLUS
	std::vector<double> getStimTimestamps(const neuron_coord & ii);

	// Enables the analog output of a neuron and ensures that it is exclusively
	void enable_aout(const neuron_coord & ii, const analog_coord & analog);

	// Enables the analog output of a neuron and ensures that it is exclusively
	void disable_aout();

	// Enables the analog output of a neuron and ensures that it is exclusively
	void disable_current_stimulus();

	// Enables the analog output of a neuron and ensures that it is exclusively
	void enable_current_stimulus(const neuron_coord & ii);

	/// Enable l1 output for neuron ii with L1Address addr
	void enable_l1_output(const neuron_coord & ii, const L1Address & addr);

	/// Disable l1 output of all neurons
	void disable_l1_output();

	/// Disable l1 output of a single neuron
	void disable_l1_output(const neuron_coord & ii);

	/// Enable firing (and membrane reset) for all neurons
	void enable_firing();

	/// Enable firing (and membrane reset) for a single neuron
	void enable_firing(const neuron_coord & ii);

	/// Disable firing (and membrane reset) for all neurons
	void disable_firing();

	/// Resets the complete HICANN configuration to default values, but keeps
	/// the connection to the wafer alive
	void clear();

	/// Clear the settings of all crossbar and synapse switches
	void clear_l1_switches();

	/// Clear the settings of all crossbar and synapse switches and synapse drivers
	void clear_l1_routing();

	/// Clear the settings of:
	///  * crossbar and synapse switches
	///  * synapse drivers
	///  * horizontal and vertical repeaters
	///  * repeater blocks
	void clear_complete_l1_routing();

	/// Clear the settings of all synapse controllers
	void clear_synapse_controllers();

	/// Switch the capacitor size on the hicann for all neurons
	/// small_capacitors: 164.2 fF
	/// big_capacitors: 2164.55 fF
	void use_big_capacitors(bool enable);

	bool get_bigcap_setting();

	/// Sets the scaling for the parameters \f$I_{gL}\f$, \f$I_{gladapt}\f$, \f$I_{radapt}\f$
	/// for the apropiated. The currents are divided by various factors:
	/// @param speedup see table below
	/// @throw std::invalid_argument when the value ist not valid
	/// | SpeedUp | \f$I_{gL}\f$ | \f$I_{gladapt}\f$ | \f$I_{radapt}\f$ |
	/// |:--------|:-------------|:------------------|:-----------------|
	/// | FAST    | 1            | 1                 | 8                |
	/// | NORMAL  | 3            | 3                 | 32               |
	/// | SLOW    | 27           | 27                | 160              |
	void set_fg_speed_up_scaling(SpeedUp speedup);

	/// Set SpeedUp factors for individual components
	void set_speed_up_gl(SpeedUp speedup);
	void set_speed_up_gladapt(SpeedUp speedup);
	void set_speed_up_radapt(SpeedUp speedup);

	SpeedUp get_speed_up_gl();
	SpeedUp get_speed_up_gladapt();
	SpeedUp get_speed_up_radapt();

	/// Store spikes to be send to the hicann via link, there are no requirements
	/// on the order of spikes
	void sendSpikes(const dnc_merger_coord & link, const std::vector<Spike> & s);

	// Sorts spiketrain of corresponding FPGA
	void sortSpikes();

	std::vector<Spike> receivedSpikes(const dnc_merger_coord & link) const;
	std::vector<Spike> sentSpikes(const dnc_merger_coord & link) const;

	// TODO: check that at least one repeater send 0 to each used mergertree output
	// void check_repeater_locking();

	/// Compare instances (does not recurse into wafer and fpga links to break the cycle)
	friend bool operator==(const HICANN & a, const HICANN & b);
	friend bool operator!=(const HICANN & a, const HICANN & b);

	/// returns the number of denmems (including defect ones)
	static size_t capacity();

	/// Connects all denmems to neurons of size with n denmens each
	/// @return vector containing firing denmems
	std::vector< ::HMF::Coordinate::NeuronOnHICANN >
	set_neuron_size(const size_t n);

	/// Connect denmems colums
	/// The top left denmem in given columns is configured as the firing denmem
	/// @return returns firing denmem
	::HMF::Coordinate::NeuronOnHICANN connect_denmems(
		::HMF::Coordinate::X first_column, ::HMF::Coordinate::X last_column);

	/// Connect denmems colums
	/// and specify the denmem that fires.
	/// the denmems in the vertical side (Y(0) or Y(1)) of the firing denmem are all interconnected.
	/// all vertical connections in the given columns are connected.
	/// Overall, this results in a comb-like structure.
	void connect_denmems(
		::HMF::Coordinate::X first_column, ::HMF::Coordinate::X last_column, ::HMF::Coordinate::NeuronOnHICANN firing_denmem);

	/// check the state of the current Configuration for dangerous settings
	/// @return true if everything is fine
	/// @note this function might not find all dangerous settings
	bool check(std::ostream & errors) const;


	/// empty string == ok
	std::string check() const;

	/// dump the current HICANN configuration into an xml file
	void xml_dump(const char * const filename, bool overwrite) const;

	/// configure a route from OutputBuffer to SynapseDriver, note that
	/// this won't check for conflicting configurations.
	/// Therefore calling clear_complete_l1_routing() in advance is recommended
	/// The function will throw if no route can be realized
	/// @param num Index to select one of the four available routes
	/// @throw std::invalid_argument When the index is out of range
	void route(::HMF::Coordinate::DNCMergerOnHICANN from,
	           ::HMF::Coordinate::SynapseDriverOnHICANN to,
	           size_t num = 0);

	/// Find the neuron connected to a given analog output. If there is none
	/// or multiple neurons connected.
	/// @throw std::runtime_error if a invalid configuration was found
	/// @throw not_found if no neuron is connencted
	::HMF::Coordinate::NeuronOnHICANN find_neuron_in_analog_output(
			analog_coord analog) const;

	/// check if HICANN knows its wafer
	bool has_wafer() const;

private:
	hicann_coord mCoordinate;
	boost::weak_ptr<FPGA> mFPGA;
	Wafer* mWafer;

	typedef boost::shared_ptr<ADCConfig> aout_t;
#ifndef PYPLUSPLUS
	halco::common::typed_array<std::optional<aout_t>, analog_coord> mADCConfig;
#endif
	PYPP_INIT(size_t mVersion, 0);
	PYPP_INIT(size_t mMinorVersion, 0);

	boost::shared_ptr<FPGA> fpga();
	boost::shared_ptr<const FPGA> fpga() const;

#ifndef PYPLUSPLUS
	Wafer& wafer();
	const Wafer& wafer() const;
#endif

	friend std::ostream& operator<<(std::ostream& os, const HICANN & h);

	friend class boost::serialization::access;
	template <class Archiver>
	void serialize(Archiver & ar, unsigned int const version);
};

} // end namespace sthal

BOOST_CLASS_EXPORT_KEY(sthal::HICANN)
BOOST_CLASS_TRACKING(sthal::HICANN, boost::serialization::track_always)
