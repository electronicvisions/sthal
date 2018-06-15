#pragma once

#include "hal/Coordinate/HMFGeometry.h"
#include "hal/HICANN/SynapseDriver.h"
#include "hal/HICANNContainer.h"

#include "sthal/macros.h"

namespace sthal {

struct SynapseRowProxy
{
	SynapseRowProxy(::HMF::HICANN::DecoderRow & d, ::HMF::HICANN::WeightRow & w) :
		decoders(d), weights(w)
	{}

	::HMF::HICANN::DecoderRow & decoders;
	::HMF::HICANN::WeightRow & weights;
};

struct SynapseRowConstProxy
{
	SynapseRowConstProxy(const ::HMF::HICANN::DecoderRow & d,
			const ::HMF::HICANN::WeightRow & w) :
		decoders(d), weights(w)
	{}

	const ::HMF::HICANN::DecoderRow & decoders;
	const ::HMF::HICANN::WeightRow & weights;
};

struct SynapseProxy
{
	SynapseProxy(::HMF::HICANN::SynapseDecoder & d, ::HMF::HICANN::SynapseWeight & w) :
		decoder(d), weight(w)
	{}

	::HMF::HICANN::SynapseDecoder & decoder;
	::HMF::HICANN::SynapseWeight & weight;
};

struct SynapseConstProxy
{
	SynapseConstProxy(const ::HMF::HICANN::SynapseDecoder & d, const ::HMF::HICANN::SynapseWeight & w) :
		decoder(d), weight(w)
	{}

	const ::HMF::HICANN::SynapseDecoder & decoder;
	const ::HMF::HICANN::SynapseWeight & weight;
};

struct SynapseArray
{
	typedef ::HMF::HICANN::SynapseDriver driver_type;
	typedef ::HMF::HICANN::WeightRow     weight_row_type;
	typedef ::HMF::HICANN::DecoderRow    decoder_row_type;

	typedef ::HMF::Coordinate::SynapseDriverOnHICANN driver_coordinate;
	typedef ::HMF::Coordinate::SynapseRowOnHICANN    row_coordinate;
	typedef ::HMF::Coordinate::SynapseOnHICANN       synapse_coordinate;

	static const size_t no_drivers = driver_coordinate::y_type::size; // TODO use enum_type, when correct
	static const size_t no_lines   = row_coordinate::size;

	STHAL_ARRAY_OPERATOR(driver_type, driver_coordinate,
		return drivers[ii.line()];);

	SynapseRowProxy operator[](row_coordinate const & row)
	{
		return SynapseRowProxy(
				decoders[row.toSynapseDriverOnHICANN().id()][
					row.toRowOnSynapseDriver()],
				weights[row]);
	}

	SynapseRowConstProxy operator[](row_coordinate const & row) const
	{
		return SynapseRowConstProxy(
				decoders[row.toSynapseDriverOnHICANN().id()][
					row.toRowOnSynapseDriver()],
				weights[row]);
	}

	SynapseProxy operator[](synapse_coordinate const & s)
	{
		const row_coordinate & row = s.toSynapseRowOnHICANN();
		return SynapseProxy(
				decoders[row.toSynapseDriverOnHICANN().id()][
					row.toRowOnSynapseDriver()][s.toSynapseColumnOnHICANN()],
				weights[row][s.toSynapseColumnOnHICANN()]);
	}

	SynapseConstProxy operator[](synapse_coordinate const & s) const
	{
		const row_coordinate & row = s.toSynapseRowOnHICANN();
		return SynapseConstProxy(
				decoders[row.toSynapseDriverOnHICANN().id()][
					row.toRowOnSynapseDriver()][s.toSynapseColumnOnHICANN()],
				weights[row][s.toSynapseColumnOnHICANN()]);
	}

	const ::HMF::HICANN::DecoderDoubleRow &
	getDecoderDoubleRow(driver_coordinate const & s) const
	{
		return decoders[s.line()];
	}

	void setDecoderDoubleRow(driver_coordinate const & s,
			const ::HMF::HICANN::DecoderDoubleRow & row)
	{
		decoders[s.line()] = row;
	}


	void clear_drivers();
	void clear_synapses();
	void set_all(::HMF::HICANN::SynapseDecoder decoder, ::HMF::HICANN::SynapseWeight weight);

	friend bool operator==(const SynapseArray & a, const SynapseArray & b);
	friend bool operator!=(const SynapseArray & a, const SynapseArray & b);
	friend std::ostream& operator<<(std::ostream& os, SynapseArray const& a);

private:
	typedef std::array< driver_type, no_drivers >  drivers_type;
	typedef std::array< weight_row_type, no_lines * 2 >    weights_type;
	typedef std::array< ::HMF::HICANN::DecoderDoubleRow, no_lines > decoders_type;

	drivers_type  drivers;
	weights_type  weights;
	decoders_type decoders;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver& ar, unsigned int const)
	{
		ar & boost::serialization::make_nvp("drivers",  drivers)
		   & boost::serialization::make_nvp("weights",  weights)
		   & boost::serialization::make_nvp("decoders", decoders);
	}
};

} // end namespace sthal

#include "sthal/macros_undef.h"
