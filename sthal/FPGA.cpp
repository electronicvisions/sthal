#include "sthal/FPGA.h"
#include "sthal/Timer.h"

#include "halco/common/iter_all.h"
#include "hal/HICANN/GbitLink.h"
#include "hal/FPGAContainer.h"
#include "sthal/HICANN.h"

using namespace ::halco::hicann::v2;
using namespace ::halco::common;

namespace sthal {

const int FPGA::dnc_freq_in_MHz = ::HMF::FPGA::DNC_frequency_in_MHz;
const double FPGA::dnc_freq = 1e6 * dnc_freq_in_MHz;
// 1Gbit/s, 2 spikes per 80 bits
const double FPGA::gbitlink_max_throughput = 25e6;

FPGA::FPGA(fpga_coord const& fpga,
		boost::shared_ptr<FPGAShared> shared) :
	mCoordinate(fpga),
	mSharedSettings(shared),
	spinnaker_enable(false),
	spinnaker_upsample_count(1),
	spinnaker_downsample_count(1)
{
	for (auto h : iter_all<HICANNOnDNC>()) {
		highspeed_hicanns.insert(h);
	}
}

void FPGA::add_hicann(const hicann_coord & h_local, const hicann_t & hicann)
{
	HICANNGlobal h(h_local, wafer());
	mDNCs[h.toDNCOnFPGA()][h.toHICANNOnDNC()] = hicann;
}

void FPGA::addSendSpikes(const hicann_coord & hicann_local,
		                 const dnc_merger_coord & dnc_merger,
					     const std::vector<Spike> & pulses)
{
	// Invalidate cache used by #getSentSpikes()
	m_sent_spikes_cache.clear();

	std::vector<FPGA::PulseEvent>& data = m_pending_send_pulses;
	size_t old_size = data.size();
	data.resize(old_size + pulses.size());

	HICANNGlobal hicann(hicann_local, wafer());
	PulseEvent::dnc_address_t  dnc(hicann.toDNCOnFPGA().value());
	PulseEvent::chip_address_t chip(hicann.toHICANNOnDNC().toEnum());
	PulseEvent::channel_t      merger(dnc_merger.value());

	std::transform(pulses.begin(), pulses.end(), data.begin() + old_size,
			[dnc, chip, merger](const Spike & s) {
				return PulseEvent(dnc, chip, merger, s.addr, size_t(s.time * dnc_freq)); }
			);
}

void FPGA::setSendSpikes(const FPGA::PulseEventContainer & events) {
	// Invalidate cache used by #getSentSpikes()
	m_sent_spikes_cache.clear();
	// Discard all pending unsorted pulse events, as `setSendSpikes` overwrites all events.
	m_pending_send_pulses.clear();
	m_send_pulses = events;
}

void FPGA::sortSpikes()
{
	m_pending_send_pulses.reserve(m_pending_send_pulses.size() + m_send_pulses.size());
	std::copy(
		m_send_pulses.data().begin(), m_send_pulses.data().end(),
		std::back_inserter(m_pending_send_pulses));
	// PulseEventContainer handles sorting.
	m_send_pulses = PulseEventContainer(std::move(m_pending_send_pulses));
	m_pending_send_pulses.clear();
}

const FPGA::PulseEventContainer& FPGA::getSendSpikes() const
{
	if (m_pending_send_pulses.empty()) {
		return m_send_pulses;
	} else {
		throw std::runtime_error("FPGA::getSendSpikes: there are spikes pending to be sent");
	}
}

void FPGA::insertReceivedPulseEvent(const FPGA::PulseEvent& event) {
	// Invalidate cache used by #getReceivedSpikes()
	m_received_spikes.clear();

	m_received_pulses.push_back(event);
}


void FPGA::setReceivedPulseEvents(pulse_event_container_type const& pulse_events)
{
	// Invalidate cache used by #getReceivedSpikes()
	m_received_spikes.clear();

	m_received_pulses = pulse_events;
}

void FPGA::setReceivedPulseEvents(pulse_event_container_type&& pulse_events)
{
	// Invalidate cache used by #getReceivedSpikes()
	m_received_spikes.clear();

	m_received_pulses = std::move(pulse_events);
}

std::vector<Spike> const& FPGA::getReceivedSpikes(
    const hicann_coord& hicann_local, const dnc_merger_coord& dnc_merger) const
{
	DNCMergerOnWafer const dnc_on_wafer(dnc_merger.toDNCMergerOnHICANN(), hicann_local);
	auto const it = m_received_spikes.find(dnc_on_wafer);
	if (it != m_received_spikes.end()) {
		return it->second;
	}

	auto& spikes = m_received_spikes[dnc_on_wafer];

	HICANNGlobal hicann(hicann_local, wafer());
	PulseEvent::dnc_address_t  dnc(hicann.toDNCOnFPGA().value());
	PulseEvent::chip_address_t chip(hicann.toHICANNOnDNC().toEnum());
	PulseEvent::channel_t      merger(dnc_merger.value());
	double t = 1.0/dnc_freq;
	for (auto& p : m_received_pulses) {
		if (p.matches(dnc, chip, merger)) {
			spikes.push_back(Spike{p.getNeuronAddress(), p.getTime() * t});
		}
	}

	return spikes;
}

std::vector<Spike> const& FPGA::getSentSpikes(
    const hicann_coord& hicann_local, const dnc_merger_coord& dnc_merger) const
{
	DNCMergerOnWafer const dnc_on_wafer(dnc_merger.toDNCMergerOnHICANN(), hicann_local);
	auto const it = m_sent_spikes_cache.find(dnc_on_wafer);
	if (it != m_sent_spikes_cache.end()) {
		return it->second;
	}

	if (!m_pending_send_pulses.empty()) {
		throw std::runtime_error("FPGA::getSentSpikes: there are spikes pending to be sent");
	}

	auto& spikes = m_sent_spikes_cache[dnc_on_wafer];

	HICANNGlobal hicann(hicann_local, wafer());
	PulseEvent::dnc_address_t  dnc(hicann.toDNCOnFPGA().value());
	PulseEvent::chip_address_t chip(hicann.toHICANNOnDNC().toEnum());
	PulseEvent::channel_t      merger(dnc_merger.value());
	double t = 1.0/dnc_freq;
	for (auto& p : m_send_pulses.data()) {
		if (p.matches(dnc, chip, merger)) {
			spikes.push_back(Spike{p.getNeuronAddress(), p.getTime() * t});
		}
	}
	return spikes;
}

auto FPGA::getReceivedSpikes() const -> pulse_event_container_type const&
{
	return m_received_pulses;
}

std::vector<FPGA::hicann_coord> FPGA::getAllocatedHICANNs() const
{
	std::vector<hicann_coord> result;
	for (auto dnc : ::halco::common::iter_all<dnc_coord>()) {
		auto dnc_on_wafer = dnc.toDNCOnWafer(coordinate());
		for (auto hicann : mDNCs[dnc].getAllocatedHicanns()) {
			result.emplace_back(hicann.toHICANNOnWafer(dnc_on_wafer));
		}
	}
	return result;
}

::halco::hicann::v2::Wafer FPGA::wafer() const
{
	return mCoordinate.toWafer();
}

FPGA::fpga_coord FPGA::coordinate() const
{
	return mCoordinate;
}

void FPGA::clearSendSpikes()
{
	// Invalidate cache used by #getSentSpikes()
	m_sent_spikes_cache.clear();

	m_send_pulses.clear();
	m_pending_send_pulses.clear();
}

void FPGA::clearReceivedSpikes()
{
	// Invalidate cache used by #getReceivedSpikes()
	m_received_spikes.clear();

	m_received_pulses.clear();
}

boost::shared_ptr<FPGAShared> FPGA::commonFPGASettings()
{
	return mSharedSettings;
}

boost::shared_ptr<const FPGAShared> FPGA::commonFPGASettings() const
{
	return mSharedSettings;
}

void FPGA::setSpinnakerEnable(bool const e)
{
	spinnaker_enable = e;
}

bool FPGA::getSpinnakerEnable() const
{
	return spinnaker_enable;
}

void FPGA::setSpinnakerUpsampleCount(size_t const u)
{
	spinnaker_upsample_count = u;
}

size_t FPGA::getSpinnakerUpsampleCount() const
{
	return spinnaker_upsample_count;
}

void FPGA::setSpinnakerDownsampleCount(size_t const d)
{
	spinnaker_downsample_count = d;
}

size_t FPGA::getSpinnakerDownsampleCount() const
{
	return spinnaker_downsample_count;
}

void FPGA::setSpinnakerRoutingTable(HMF::FPGA::SpinnRoutingTable const t)
{
	spinnaker_routing_table = t;
}

HMF::FPGA::SpinnRoutingTable FPGA::getSpinnakerRoutingTable() const
{
	return spinnaker_routing_table;
}

void FPGA::setHighspeed(const halco::hicann::v2::HICANNOnDNC& highspeed_hicann, bool use_hs)
{
	if (use_hs) {
		highspeed_hicanns.insert(highspeed_hicann);
	} else {
		highspeed_hicanns.erase(highspeed_hicann);
	}
}

bool FPGA::getHighspeed(const halco::hicann::v2::HICANNOnDNC& highspeed_hicann) const
{
	return highspeed_hicanns.count(highspeed_hicann);
}

void FPGA::setBlacklisted(const halco::hicann::v2::HICANNOnDNC& blacklisted_hicann, bool blacklist)
{
	if (blacklist) {
		blacklisted_hicanns.insert(blacklisted_hicann);
	} else {
		blacklisted_hicanns.erase(blacklisted_hicann);
	}
}

bool FPGA::getBlacklisted(const halco::hicann::v2::HICANNOnDNC& blacklisted_hicann) const
{
	return blacklisted_hicanns.count(blacklisted_hicann);
}

bool operator==(FPGA const& a, FPGA const& b)
{
	return (a.mCoordinate == b.mCoordinate)
		&& (a.mDNCs == b.mDNCs)
		&& (a.m_send_pulses == b.m_send_pulses)
		&& (a.m_received_pulses == b.m_received_pulses)
		&& (a.m_pending_send_pulses == b.m_pending_send_pulses)
		&& ((static_cast<bool>(a.mSharedSettings) == static_cast<bool>(b.mSharedSettings)) &&
			(static_cast<bool>(a.mSharedSettings) && ((*a.mSharedSettings) == (*b.mSharedSettings))))
		&& (a.spinnaker_enable == b.spinnaker_enable)
		&& (a.spinnaker_upsample_count == b.spinnaker_upsample_count)
		&& (a.spinnaker_downsample_count == b.spinnaker_downsample_count)
		&& (a.spinnaker_routing_table == b.spinnaker_routing_table)
		&& (a.highspeed_hicanns == b.highspeed_hicanns)
		&& (a.blacklisted_hicanns == b.blacklisted_hicanns)
	;
}

bool operator!=(FPGA const& a, FPGA const& b)
{
	return !(a == b);
}

bool FPGA::hasOutboundMergers() const
{
	for (HICANNOnWafer const hicann_c : getAllocatedHICANNs()) {
		HICANNOnDNC const hicann_on_dnc_c = hicann_c.toHICANNOnDNC();
		DNCOnFPGA const dnc_c = HICANNGlobal(hicann_c, wafer()).toDNCOnFPGA();
		for (auto channel : iter_all<GbitLinkOnHICANN>()) {
			if (mDNCs[dnc_c][hicann_on_dnc_c]->layer1[channel] ==
			    HMF::HICANN::GbitLink::Direction::TO_DNC) {
				return true;
			}
		}
	}
	return false;
}

} // end namespace sthal
