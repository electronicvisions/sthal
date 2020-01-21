#pragma once

#include "halco/hicann/v2/fwd.h"
#include "hal/HICANNContainer.h"

#include "sthal/macros.h"

namespace sthal {

struct Neurons {
	Neurons();
	typedef ::halco::hicann::v2::QuadOnHICANN   quad_coordinate;
	typedef ::halco::hicann::v2::NeuronOnHICANN neuron_coordinate;
	typedef ::HMF::HICANN::NeuronQuad         quad_type;
	typedef ::HMF::HICANN::Neuron             neuron_type;
	typedef ::HMF::HICANN::NeuronConfig       config_type;

	STHAL_ARRAY_OPERATOR(quad_type, quad_coordinate,
		    return mQuads[ii];)
	STHAL_ARRAY_OPERATOR(neuron_type, neuron_coordinate,
			return mQuads[ii.toQuadOnHICANN()][ii.toNeuronOnQuad()];)

	/// checks whether the analog IO settings are fine.
	/// e.g. whether some membranes are shortened.
	bool check_analog_io();

	/// checks whether the current input settings are fine.
	/// e.g. whether some membranes are shortened.
	bool check_current_io();

	/// checks whether neurons with enabled spl1 output are activated for firing
	/// neuron.enable_spl1_output() == true should also have activate_firing = true.
	/// otherwise the spl1_output is never triggered.
	/// See hal/HICANNContainer class HMF::HICANN::Neuron for doc.
	bool check_enable_spl1_output(std::ostream & errors) const;

	friend bool operator==(const Neurons & a, const Neurons & b);
	friend bool operator!=(const Neurons & a, const Neurons & b);

	bool check(std::ostream & errors) const;

	config_type config;
private:
	typedef std::array<quad_type, quad_coordinate::size> neurons_type;

	bool check_denmem_connectivity(std::ostream & errors) const;

	neurons_type mQuads;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const)
	{
		using namespace boost::serialization;
		ar & make_nvp("quads", mQuads)
		   & make_nvp("config", config);
	}

	friend std::ostream& operator<<(std::ostream& out, Neurons const& obj);
};

}

#include "sthal/macros_undef.h"
