#pragma once

#include <ostream>
#include <string>

#include <boost/serialization/serialization.hpp>

namespace sthal
{

/// global ESS settings
struct ESSConfig
{
    ESSConfig();

	/// speedup factor and timestep are used to set the integration time step and recording time step of the hardware neurons
	/// The hardware integration time step hw_time_step is at maximum 0.01 ms / speedup.
	/// if timestep < 0.01 ms, the hw_time_step is set to timestep / speedup.
    double speedup; /// speed

	/// biological time step in ms (default: 0.1)
    double timestep;

	/// enable fixed pattern weight distortion (default: false)
	/// if true, each synaptic weight will be multiplied by a distortion factor D,
	/// which is drawn from a Normal (Gaussian) distribution with mean=1. and sigma=weight_distortion.
	/// D is clipped to non-negative values.
    bool enable_weight_distortion;

	/// strength of weight distortion, see above (default: 0., range: [0,+inf[)
    double weight_distortion;

	/// enable time accurate merger tree (default: true)
    bool enable_timed_merger;

	/// enable spike debugging (default: false)
    bool enable_spike_debugging;

	/// (python-)file to which summary about lost events is written.
	/// If not specified, summary is not written to file
	std::string pulse_statistics_file;

	/// Path to directory containing calibration data as xml-files (default : "")
	/// The calibration is looked up for each HICANN in xml files with pattern
	/// "w<W>-h<H>.xml", where <W> is the wafer id and <H> the HICANN id, e.g.:
	/// w0-h84.xml or w3-h276.xml
	/// If a calibration is available for a HICANN, it is used for the reverse transformation from
	/// hardware to {neuron,synapse} model parameters, which are then used in simulation.
	/// If string is empty or no XML file is found for a HICANN, the default calibration is used.
	/// @note Make sure to provide corresponding xml files for the wafer you are using.
	std::string calib_path;

	bool operator==(ESSConfig const& rhs) const;
	bool operator!=(ESSConfig const& rhs) const;

#ifndef PYPLUSPLUS
private:
	/// serialization
	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, unsigned int const);
#endif // PYPLUSPLUS
};

std::ostream& operator<<(std::ostream& out, ESSConfig const& obj);

} // end namespace sthal

#ifndef PYPLUSPLUS
#include <boost/serialization/export.hpp>
BOOST_CLASS_EXPORT_KEY(::sthal::ESSConfig)
#endif
