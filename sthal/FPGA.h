#pragma once

#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/set.hpp>
#ifndef PYPLUSPLUS
#include <unordered_map>
#endif // !PYPLUSPLUS

#include "sthal/DNC.h"
#include "sthal/Spike.h"
#include "sthal/FPGAShared.h"
#include "hal/FPGAContainer.h"
#include "halco/hicann/v2/l1.h"

#include "sthal/macros.h"

namespace sthal {

class HICANN;

class FPGA
{
public:
	typedef boost::shared_ptr<sthal::HICANN>    hicann_t;
	typedef ::halco::hicann::v2::FPGAGlobal       fpga_coord;
	typedef ::halco::hicann::v2::HICANNOnWafer    hicann_coord;
	typedef ::halco::hicann::v2::HICANNGlobal     hicann_coord_global;
	typedef ::halco::hicann::v2::DNCOnFPGA        dnc_coord;
	typedef ::halco::hicann::v2::GbitLinkOnHICANN dnc_merger_coord;

	typedef ::HMF::FPGA::PulseEvent PulseEvent;
	typedef std::vector<PulseEvent> pulse_event_container_type;
	typedef ::HMF::FPGA::PulseEventContainer PulseEventContainer;

	FPGA(fpga_coord const& fpga = fpga_coord(), boost::shared_ptr<FPGAShared> shared = boost::shared_ptr<FPGAShared>());

	void add_hicann(const hicann_coord &, const hicann_t & );

	STHAL_ARRAY_OPERATOR(DNC, dnc_coord, return mDNCs[ii];)
	STHAL_ARRAY_OPERATOR(
		hicann_t, hicann_coord,
		hicann_coord_global h(ii, wafer());
		return mDNCs[h.toDNCOnFPGA()][h.toHICANNOnDNC()];)

	void addSendSpikes(const hicann_coord & hicann,
			           const dnc_merger_coord & dnc_merger,
					   const SpikeVector & pulses);

	/// set the spikes that will be sent to the hardware, overwrites all existing events
	void setSendSpikes(const PulseEventContainer & events);

	/// sort Playbackmemory events ascending by time
	void sortSpikes();

	/**
	 * @brief Return sent spikes for the whole FPGA. sortSpikes() needs
	 * to be called first or else the function will throw
	 */
	const PulseEventContainer & getSendSpikes() const;

	/**
	 * @brief Insert pulse event of received spike.
	 * @note This invalidates the cache used by #getReceivedSpikes().
	 */
	void insertReceivedPulseEvent(const PulseEvent& event);

	/**
	 * @brief Set received pulse events.
	 * @note This invalidates the cache used by #getReceivedSpikes().
	 */
	void setReceivedPulseEvents(pulse_event_container_type const& pulse_events);

#ifndef PYPLUSPLUS
	/**
	 * @brief Set received pulse events.
	 * @note This invalidates the cache used by #getReceivedSpikes().
	 */
	void setReceivedPulseEvents(pulse_event_container_type&& pulse_events);
#endif // !PYPLUSPLUS

	/**
	 * @brief Return received spikes for the given DNC merger.
	 */
	SpikeVector const& getReceivedSpikes(
	    const hicann_coord& hicann, const dnc_merger_coord& dnc_merger) const;

	/**
	 * @brief Return sent spikes for the given DNC merger. sortSpikes() needs
	 * to be called first or else the function will throw
	 */
	SpikeVector const& getSentSpikes(
	    const hicann_coord& hicann, const dnc_merger_coord& dnc_merger) const;

	/// get received spikes
	pulse_event_container_type const& getReceivedSpikes() const;

	std::vector<hicann_coord> getAllocatedHICANNs() const;

	::halco::hicann::v2::Wafer wafer() const;
	fpga_coord coordinate() const;

	/// Clear spikes
	void clearSendSpikes();
	void clearReceivedSpikes();

	/// Get settings shared between all FPGAs
	boost::shared_ptr<FPGAShared> commonFPGASettings();

#ifndef PYPLUSPLUS
	boost::shared_ptr<const FPGAShared> commonFPGASettings() const;
#endif

	/// SpiNNakerIF-specific things
	void setSpinnakerEnable(bool const);
	bool getSpinnakerEnable() const;
	void setSpinnakerUpsampleCount(size_t const);
	size_t getSpinnakerUpsampleCount() const;
	void setSpinnakerDownsampleCount(size_t const);
	size_t getSpinnakerDownsampleCount() const;
	void setSpinnakerRoutingTable(HMF::FPGA::SpinnRoutingTable const);
	HMF::FPGA::SpinnRoutingTable getSpinnakerRoutingTable() const;

	/// use highspeed links for given HICANN
	void setHighspeed(const halco::hicann::v2::HICANNOnDNC& highspeed_hicann, bool use_hs);
	/// get if highspeed links are used
	bool getHighspeed(const halco::hicann::v2::HICANNOnDNC& highspeed_hicann) const;

	/// blacklisted given HICANN
	void setBlacklisted(const halco::hicann::v2::HICANNOnDNC& blacklisted_hicann, bool blacklist);
	/// get if HICANN is blacklisted
	bool getBlacklisted(const halco::hicann::v2::HICANNOnDNC& blacklisted_hicann) const;

	// return if has at least one HICANN configured to send spikes back
	bool hasOutboundMergers() const;

private:
	fpga_coord mCoordinate;
	std::array<DNC, dnc_coord::size> mDNCs;
	PulseEventContainer m_send_pulses;
	/**
	 * @brief Yet to-be-sorted spikes that will be lazily inserted to `m_send_pulses`.
	 * @see @sortSpikes() which merges the pending pulses into `m_send_pulses`.
	 * This is in place as an optimization for many consecutive calls to `addSendSpikes`,
	 * as sorting can happen after all spikes are added.  Spikes are not directly added
	 * to `m_send_pulses`, as `PulseEventContainer` has the invariant that its contents
	 * are in a time-sorted order at all times.
	 */
	pulse_event_container_type m_pending_send_pulses;
	pulse_event_container_type m_received_pulses;

#ifndef PYPLUSPLUS
	mutable std::unordered_map<halco::hicann::v2::DNCMergerOnWafer, SpikeVector> m_received_spikes;
	mutable std::unordered_map<halco::hicann::v2::DNCMergerOnWafer, SpikeVector> m_sent_spikes_cache;
#endif // !PYPLUSPLUS
	boost::shared_ptr<FPGAShared> mSharedSettings;

	/// SpiNNaker interface settings
	bool spinnaker_enable; // enable or disable spinnaker
	// downstream (to chip) spike train multiplier
	size_t spinnaker_upsample_count;
	// upstream (from chip) spike train divider
	size_t spinnaker_downsample_count;
	// routing table (spinnaker address <=> pulse address)
	HMF::FPGA::SpinnRoutingTable spinnaker_routing_table;

	std::set<halco::hicann::v2::HICANNOnDNC> highspeed_hicanns;
	std::set<halco::hicann::v2::HICANNOnDNC> blacklisted_hicanns;

	friend class boost::serialization::access;
	template<typename Archiver>
	void serialize(Archiver & ar, unsigned int const version)
	{
		using boost::serialization::make_nvp;
		ar & make_nvp("coordinate", mCoordinate)
		   & make_nvp("dncs", mDNCs)
		   & make_nvp("send_pulses", m_send_pulses)
		   & make_nvp("received_pulses", m_received_pulses)
		   & make_nvp("pending_send_pulses", m_pending_send_pulses);
		if (version == 0)
		{
			//mSharedSettings.reset(new FPGAShared());
		}
		else
		{
			ar & make_nvp("fpga_shared_settings", mSharedSettings);
		}
		if (version >= 2) {
			// version == 0 gets defaults by default constructor
			ar & make_nvp("spinnaker_enable", spinnaker_enable)
			   & make_nvp("spinnaker_upsample_count", spinnaker_upsample_count)
			   & make_nvp("spinnaker_downsample_count", spinnaker_downsample_count)
			   & make_nvp("spinnaker_routing_table", spinnaker_routing_table);
		}
		if (version >= 3) {
			ar & make_nvp("highspeed_hicanns", highspeed_hicanns);
		}
		if (version >= 4) {
			ar & make_nvp("blacklisted_hicanns", blacklisted_hicanns);
		}
	}

public:
	/// DNC frequency, used to calculate spike timestamps
	static const int dnc_freq_in_MHz;
	static const double dnc_freq;
	/// maximum number of events
	static const double gbitlink_max_throughput;

	friend bool operator==(FPGA const& a, FPGA const& b);
	friend bool operator!=(FPGA const& a, FPGA const& b);
};

} // end namespace sthal

#include "sthal/macros_undef.h"

BOOST_CLASS_VERSION(sthal::FPGA, 4)
BOOST_CLASS_TRACKING(sthal::FPGA, boost::serialization::track_always)
