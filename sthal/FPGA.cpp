#include "sthal/FPGA.h"
#include "sthal/Timer.h"

#include "hal/Coordinate/iter_all.h"

using namespace ::HMF::Coordinate;

namespace sthal {

const int FPGA::dnc_freq_in_MHz = 250;
const double FPGA::dnc_freq = 1e6 * dnc_freq_in_MHz;

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
	PulseEvent::chip_address_t chip(hicann.toHICANNOnDNC().id());
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
		throw std::runtime_error("Spike events are not sorted, first call sortSpikes()");
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
	PulseEvent::chip_address_t chip(hicann.toHICANNOnDNC().id());
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
		throw std::runtime_error("Spike events are not sorted, first call sortSpikes()");
	}

	auto& spikes = m_sent_spikes_cache[dnc_on_wafer];

	HICANNGlobal hicann(hicann_local, wafer());
	PulseEvent::dnc_address_t  dnc(hicann.toDNCOnFPGA().value());
	PulseEvent::chip_address_t chip(hicann.toHICANNOnDNC().id());
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
	for (auto dnc : ::HMF::Coordinate::iter_all<dnc_coord>()) {
		auto dnc_on_wafer = dnc.toDNCOnWafer(coordinate());
		for (auto hicann : mDNCs[dnc].getAllocatedHicanns()) {
			result.emplace_back(hicann.toHICANNOnWafer(dnc_on_wafer));
		}
	}
	return result;
}

::HMF::Coordinate::Wafer FPGA::wafer() const
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

void FPGA::setHighspeed(const HMF::Coordinate::HICANNOnDNC& highspeed_hicann, bool use_hs)
{
	if (use_hs) {
		highspeed_hicanns.insert(highspeed_hicann);
	} else {
		highspeed_hicanns.erase(highspeed_hicann);
	}
}

bool FPGA::getHighspeed(const HMF::Coordinate::HICANNOnDNC& highspeed_hicann) const
{
	return highspeed_hicanns.count(highspeed_hicann);
}

} // end namespace sthal
