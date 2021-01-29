#!/usr/bin/env python

from PysthalTest import PysthalTest, hardware, expensive
import unittest

import numpy
import numpy as np
import pysthal
import pyhalbe
import pylogging
from pyhalco_common import iter_all, Enum, top, bottom, left, right
import pyhalco_hicann_v2 as C

FPGA_FREQ=250e6
MAX_TIMESTAMP = (2**15)/FPGA_FREQ  # counter size / FPGA frequency


class UpdateAoutConfigurator(pysthal.HICANNConfigurator):
    def config_fpga(self, handle, fpga):
        pass

    def config(self, fpga, hicann, h):
        self.config_neuron_quads(hicann, h)
        self.config_analog_readout(hicann, h)
        self.flush_hicann(hicann)

class NoResetConfigurator(pysthal.HICANNConfigurator):
    def config_fpga(self, handle, fpga):
        pass


class FastConfiguratorWithFPGABG(pysthal.HICANNConfigurator):
    """Workaround because the FPGA BG is currently not supported"""
    def __init__(self, fpga_bg):
        super(FastConfiguratorWithFPGABG, self).__init__()
        self.fpga_bg = fpga_bg

    def config_fpga(self, handle, fpga):
        super(FastConfiguratorWithFPGABG, self).config_fpga(handle, fpga)

        for dnc_c in iter_all(C.DNCOnFPGA):
            if handle.dnc_active(dnc_c):
                pyhalbe.FPGA.set_fpga_background_generator(handle, dnc_c, self.fpga_bg)


class NoneConfigurator(pysthal.HICANNConfigurator):
    def config(self, fpga, hicann, h):
        pass


class NoopConfigurator(pysthal.HICANNConfigurator):
    def config_fpga(self, x, y):
        pass

    def config(self, fpga, hicann, h):
        pass


class RerunConfigurator(pysthal.HICANNConfigurator):
    def config(self, fpga, handle, data):

        hicann_handle = pysthal.VectorHICANNHandle()
        hicann_handle.append(handle)

        pyhalbe.HICANN.init(handle, False)
        self.config_fg_stimulus(handle, data)
        self.config_synapse_array(handle, data)
        self.config_neuron_quads(handle, data)
        self.config_phase(handle, data)
        self.config_gbitlink(handle, data)

        self.config_synapse_switch(handle, data)
        self.config_stdp(handle, data)
        self.config_crossbar_switches(handle, data)
        self.config_repeater(handle, data)
        self.sync_command_buffers(fpga, hicann_handle)

        data.repeater.disable_dllreset()
        self.config_repeater_blocks(handle, data)
        self.sync_command_buffers(fpga, hicann_handle)

        self.config_merger_tree(handle, data)
        self.config_dncmerger(handle, data)
        self.config_background_generators(handle, data)
        self.flush_hicann(handle)
        self.config_synapse_drivers(handle, data)

        data.synapse_controllers.disable_dllreset()
        self.config_synapse_controllers(handle, data)
        self.sync_command_buffers(fpga, hicann_handle)

        self.config_neuron_config(handle, data)
        self.config_neuron_quads(handle, data)
        self.config_analog_readout(handle, data)
        self.flush_hicann(handle)


class TestSingleHICANN(PysthalTest):

    logger = pylogging.get("sthal.TestSingleHICANN")

    def setUp(self):
        super(TestSingleHICANN, self).setUp()

        pylogging.set_loglevel(pylogging.get("Default"), pylogging.LogLevel.ERROR)
        pylogging.set_loglevel(pylogging.get("sthal"), pylogging.LogLevel.INFO)
        pylogging.set_loglevel(pylogging.get("sthal.TestSingleHICANN"), pylogging.LogLevel.INFO)

        if None in (self.WAFER, self.HICANN):
            return

        self.wafer_c = C.Wafer(self.WAFER)
        self.hicann_c = C.HICANNOnWafer(self.HICANN)

        self.w = pysthal.Wafer(self.wafer_c)
        self.h = self.w[self.hicann_c]

        self.FPGA_HICANN_DELAY = self.w.commonFPGASettings().getFPGAHICANNDelay()

        self.addCleanup(self.w.disconnect)

    def connect(self):
        self.w.connect(pysthal.MagicHardwareDatabase())
        self.assertEqual(self.h.get_version(), self.HICANN_VERSION)

    def run_experiment(self, time, cfg=None):
        if self.RUN:
            self.connect()
            if cfg is None:
                cfg = pysthal.HICANNConfigurator()
            self.w.configure(cfg)
            if time > 0.0:
                runner = pysthal.ExperimentRunner(time)
                self.w.start(runner)

    def start_runner(self, time):
        runner = pysthal.ExperimentRunner(time)
        self.w.start(runner)

    def switch_aout(self):
        if self.RUN:
            cfg = UpdateAoutConfigurator()
            self.w.configure(cfg)

    def route(self, output_buffer, driver, route=0):
        """Attempts to connect an merger tree output with an synapse driver
        via L1 network.
        This routine also configure the SPL1 repeater in between.
        Between each output buffer and each synapse driver exists four
        possible routes.

        Note: No checks on consistency or to attempt collisions with other routes
        are done!!!

        output_buffer: OutputBufferOnHICANN
        driver: SynapseDriverOnHICANN
        route: int selects which rout to take

        returns: None
        """
        assert(route >= 0 and route <= 4)
        self.h.route(output_buffer, driver, route)

    def config_driver(self, driver_c, decoder=pyhalbe.HICANN.DriverDecoder(0)):
        """Configure a synapse drivers in "default" mode.

        driver_c: SynapseDriverOnHICANN driver to configure
        decoder: DriverDecoder Decoder value to use for all
            four decoders of the driver
        """

        driver = self.h.synapses[driver_c]
        driver.set_l1()
        driver[top].set_decoder(top, decoder)
        driver[top].set_decoder(bottom, decoder)
        # Set gmax divisor to 1
        driver[top].set_gmax_div(left, 1)
        driver[top].set_gmax_div(right, 1)
        # Connect to the "excitatory" synaptic input
        driver[top].set_syn_in(left, 1)
        driver[top].set_syn_in(right, 0)
        driver[top].set_gmax(0)
        # Use same configuration for top part
        driver[bottom] = driver[top]

    @staticmethod
    def find_spikes_in_preout(dt, data):
        th = 0.4
        tmp = np.where((data >= th)[:-1] != (data >= th)[1:])[0]
        tmp = tmp[:len(tmp)//2*2]
        spike_pos = tmp.reshape((len(tmp)//2, 2))
        positions = []
        for begin, end in spike_pos:
            begin, end = begin - 1, end + 1
            t = np.arange(begin*dt, end*dt, dt)
            pos = np.dot(data[begin:end], t) / np.sum(data[begin:end])
            positions.append(pos)
        return np.array(positions)

    @staticmethod
    def almostEqual(a, b, rel_err):
        return (np.abs(a - b) / np.maximum(a, b)) < rel_err

    @staticmethod
    def relative_error(a, b):
        return abs(a - b) / max(a, b)

    def increase_switch_limit(self, rows=None, columns=None):

        settings = pysthal.Settings.get()

        if columns:
            old = settings.synapse_switches.max_switches_per_column_per_side
            def reset_switch_limit():
                settings.synapse_switches.max_switches_per_column_per_side = old

            self.addCleanup(reset_switch_limit)
            settings.synapse_switches.max_switches_per_column_per_side = columns

            old = settings.crossbar_switches.max_switches_per_column
            def reset_switch_limit():
                settings.crossbar_switches.max_switches_per_column = old

            self.addCleanup(reset_switch_limit)
            settings.crossbar_switches.max_switches_per_column = columns

        if rows:
            old = settings.synapse_switches.max_switches_per_row
            def reset_switch_limit():
                settings.synapse_switches.max_switches_per_row = old

            self.addCleanup(reset_switch_limit)
            settings.synapse_switches.max_switches_per_row = rows

            old = settings.crossbar_switches.max_switches_per_row
            def reset_switch_limit():
                settings.crossbar_switches.max_switches_per_row = old

            self.addCleanup(reset_switch_limit)
            settings.crossbar_switches.max_switches_per_row = rows

    @hardware
    def test_analog_readout(self):
        self.logger.INFO("Starting test_analog_readout")
        self.w.connect(pysthal.MagicHardwareDatabase())
        readout = self.h.analogRecorder(C.AnalogOnHICANN(0))
        readout.setRecordingTime(1.0e-2)
        x = readout.record()
        x = readout.record()
        y = readout.record()
        del x, y
        # assertion: this test does not raise any Exception

#    @parameterized_test(20, 100, 1000, 10000)
#    def test_Background_frequences_against_ADC_samplerate(self, bg_period):
#        """This test sends background pulses to the synapse drivers 111 and
#        112 and then checks the pro_out signal of the synapse driver to see
#        if they match the frequency of the background generator"""

    @hardware
    def test_bg_event_filter(self):
        """Test filtering of background events. Activate bg generators and readout trace.
        Should be empty with active event filter and non empty without filter"""

        self.logger.INFO("Starting test_bg_event_filter")
        bg_period = 500
        # arbitrarily long enough runtime to capture background events
        runtime = 1. / pysthal.FPGA.dnc_freq * bg_period * 100

        for bg in iter_all(C.BackgroundGeneratorOnHICANN):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(bg_period)
            generator.address(pyhalbe.HICANN.L1Address(0))

        direction = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = direction

        self.connect()
        cfg = pysthal.HICANNConfigurator()
        self.w.configure(cfg)
        runner = pysthal.ExperimentRunner(runtime, True)
        self.w.start(runner)
        received = self.h.receivedSpikes(C.GbitLinkOnHICANN(0))
        self.assertEqual(received.size, 0)

        runner.drop_background_events(False)
        self.w.start(runner)
        received = self.h.receivedSpikes(C.GbitLinkOnHICANN(0))

        self.assertTrue(received.size > 0)

    @hardware
    def test_background_frequences_against_ADC_samplerate(self, bg_period=1000):
        """This test sends background pulses to the synapse drivers 111 and
        112 and then checks the pro_out signal of the synapse driver to see
        if they match the frequency of the background generator"""

        self.increase_switch_limit(columns=2)

        self.logger.INFO("Starting test_background_frequences_against_ADC_samplerate")
        # The bg send a spike every period + 1 clk cycles
        pll_freq = self.w.commonFPGASettings().getPLL()
        bg_freq = pll_freq/(bg_period + 1)

        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.RIGHT_ONLY
            m.slow = False
            m.loopback = False

        for bg in iter_all(C.BackgroundGeneratorOnHICANN):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(bg_period)
            generator.address(pyhalbe.HICANN.L1Address(0))

        for driver in self.PREOUT_DRIVERS:
            self.config_driver(driver)
            self.route(self.PREOUT_OUTPUT, driver)
        self.h.analog.set_preout(C.AnalogOnHICANN(0))
        self.h.analog.set_preout(C.AnalogOnHICANN(1))

        self.run_experiment(0.0)

        def eval(dt, data):
            positions = self.find_spikes_in_preout(dt, data)
            isi = positions[1:] - positions[:-1]
            mean, std = np.mean(isi), np.std(isi)
            return 1.0/mean, np.std(isi)/np.mean(isi)

        def check(ac):
            sd_str = ("upper" if ac.value() == 0 else "lower") + " synapse driver"
            readout = self.h.analogRecorder(ac)
            readout.setRecordingTime(1.0e-2)
            readout.record()
            dt, data = readout.getTimestamp(), readout.trace()
            readout.freeHandle()
            freq, qual = eval(dt, data)
            print("Measured frequencies: ", freq, "on", sd_str)
            # FOR DEBUG
            # np.save("synapse_driver_%i_%i_%i_%i.npy" % (bg_period, self.wafer_c, self.hicann_c.toEnum(), ac.value()), (readout.getTimestamps(), data) )
            err = ""
            if not self.almostEqual(freq, bg_freq, 1e-4):
                err += "%s: Meassured background frequency wrong (freq = %f, expeced %f) on the " % (ac, freq, bg_freq)
                err += sd_str + "\n"
            # CK: Is this ok? We are more friendly to faster gb's
            if qual > (0.01 if bg_period < 50 else 0.001):
                err += "%s: The preout pulses of the " + sd_str + " are not regularly distributed (qual = %f)\n" % (ac, qual)
            return freq, err

        freq0, err0 = check(C.AnalogOnHICANN(0))
        freq1, err1 = check(C.AnalogOnHICANN(1))
        err = err0 + err1

        if not self.almostEqual(freq0, freq1, 1e-5):
            err += "The messured frequencies between upper and lower syndriver don't macht %f != %f" % (freq0, freq1)

        self.assertFalse(err, err)

        # FOR DEBUG
        # # showing stored results quickly
        # for ii in (20, 100, 1000, 10000):
        #    for jj in (0, 1):
        #        t, x = numpy.load("synapse_driver_%i_%i.npy" % (ii, jj))
        #            print "%i_%i:" % (ii, jj), eval(t,x)
        #
        # # checking quality of pulse detection
        # positions = find_pos(t,x)
        # for a, b in positions.reshape(len(positions)//2, 2):
        #     pylab.bar(a, 1.0, width=(b-a), color='red', alpha=0.65)
        # pylab.plot(t, x); pylab.show()

    @hardware
    def test_Background_frequences_against_L2_spiketimes(self):
        """Set each background generator to regular firing mode and a random
        frequency. Then checks if the spike times read back via DNC match
        the set frequencies"""

        self.logger.INFO("Starting test_Background_frequences_against_L2_spiketimes")
        from random import Random
        rng = Random()
        # Configure mergers and DNC to output spikes to the dnc
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.RIGHT_ONLY
            m.slow = False
            m.loopback = False

        direction = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = direction

        addrs     = [pyhalbe.HICANN.L1Address(a) for a in rng.sample(range(64), 8)]
        seeds     = [rng.randrange(2**16) for ii in range(8)]
        periods   = [rng.randrange(500, 3000) for ii in range(8)]

        for ii, bg in enumerate(iter_all(C.BackgroundGeneratorOnHICANN)):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.seed(seeds[ii])
            generator.period(periods[ii])
            generator.address(addrs[ii])

        #self.h.layer1[C.BackgroundGeneratorOnHICANN(fast)].enable(True)

        exp_duration = 0.01
        self.run_experiment(exp_duration)

        # Collect data
        no_spikes = []
        spiketimes = []
        for ii, channel in enumerate(iter_all(C.GbitLinkOnHICANN)):
            received = self.h.receivedSpikes(channel)
            times, _ = received.T
            no_spikes.append(len(received))
            spiketimes.append(times)

        # FOR DEBUG
        # data = { "periods" : periods, "spiketimes" : spiketimes }
        # np.save("spiketimes.npy", data)

        def get_freq(period, spikes):
            bg_freq = 100e6/(period + 1)
            # Isi
            isi = spikes[1:] - spikes[:-1]
            mean, std = np.mean(isi), np.std(isi)
            freq_isi, qual_isi  = 1.0/mean, std/mean

            # Fourier
            slots_per_s = int(10e6)
            t = spikes[-1]
            cnt = np.zeros(int(slots_per_s * t) + 2, dtype=np.int8)
            cnt[np.array(np.around(spikes * slots_per_s), dtype=np.int64)] += 1
            fourier = np.fft.rfft(cnt)
            freq = np.fft.fftfreq(len(cnt), 1.0/slots_per_s)[:len(cnt)-1]
            index = np.where((freq[:-1] < bg_freq) & (freq[1:] > bg_freq))[0][0]
            w = np.arange(index-7, index+8)
            window = np.log(abs(fourier[w]))
            freq_fourier = np.dot(window, freq[w])/np.sum(window)
            return bg_freq, freq_isi, qual_isi, freq_fourier

        err = ""
        err_ = "Missmatch on background generator "
        for ii in range(8):
            if len(spiketimes[ii]) == 0:
                err += err_ + "%i no spikes on channel\n" % ii
                continue

            # Verify that recorded data spans at most the length
            # of the experiment duration (incl. a bit of grace time)
            exp_dur_grace_factor=1.2
            maxtimediff = max(spiketimes[ii]) - min(spiketimes[ii])
            if maxtimediff > exp_duration*exp_dur_grace_factor:
                err += err_ + "maximum time difference of {} s = {} s - {} s is greater than experiment duration ({} s)\n".format(
                    maxtimediff, max(spiketimes[ii]), min(spiketimes[ii]), exp_duration*exp_dur_grace_factor)
            elif max(spiketimes[ii]) > exp_duration*exp_dur_grace_factor:
                err += err_ + "maximum spike time of {} s is greater than experiment duration of {} s\n".format(
                    max(spiketimes[ii]), exp_duration*exp_dur_grace_factor)
            else:
                bg_freq, freq_isi, qual_isi, freq_fourier = get_freq(periods[ii], spiketimes[ii])
                if not self.almostEqual(bg_freq, freq_fourier, 4e-3):
                    err += err_ + "%i expected %f (meassured isi: %f, fourier: %f)\n" % (ii, bg_freq, freq_isi, freq_fourier)

        self.assertFalse(err, err)

    @hardware
    def test_Background_addresses(self):
        """Set each background generators to fire regularly or in random mode
        and checks the address of the spike received via FPGA.
        In the current HICANN is bug, that the address of the background
        generator in random mode is always 0.
        """

        self.logger.INFO("Starting test_Background_addresses")
        from random import Random
        rng = Random()

        # Configure mergers and DNC to output spikes to the dnc
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.RIGHT_ONLY
            m.slow = False
            m.loopback = False

        direction = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = direction

        addrs     = rng.sample(range(1,64), 8)
        seeds     = [rng.randrange(2**16) for ii in range(8)]
        periods   = [rng.randrange(500, 3000) for ii in range(8)]

        for ii, bg in enumerate(iter_all(C.BackgroundGeneratorOnHICANN)):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(bool(ii % 2))
            generator.seed(seeds[ii])
            generator.period(periods[ii])
            generator.address(pyhalbe.HICANN.L1Address(addrs[ii]))

        self.run_experiment(0.01)

        # Collect data
        no_spikes = []
        received_addresses = []
        for ii, channel in enumerate(iter_all(C.GbitLinkOnHICANN)):
            received = self.h.receivedSpikes(channel)
            _, tmp = received.T
            no_spikes.append(len(tmp))
            received_addresses.append(list(set(tmp.astype(numpy.int64))))

        err = []
        for ii in range(8):
            if no_spikes[ii] == 0:
                err.append("Received no spikes from background generator %i" % ii)
            elif received_addresses[ii] != [addrs[ii]]:
                msg = ("Background Generator %i is sending with address %s, "
                       "expecting %i" % (ii, received_addresses[ii], addrs[ii]))
                if bool(ii % 2) and self.HICANN_VERSION <= 4:
                    print("Expected error: ", msg)
                else:
                    err.append(msg)

        self.assertFalse(err, "\n" + "\t\n".join(err))


    def hicann_loopback_impl(self, no_spikes, no_spike_runs, no_empty_runs=10, dt=3e-6, drops_expected=False):
        """
        Send spikes to the HICANN and read them back via hicann loopback.

        This test send spikes to 4 highspeed links of the HICANN and reads
        them back over the other 4 via loopback. It is checked that all spikes
        return to the correct link, with the correct address.
        This is repeated multipe times, without reconfiguring the wafer. It is
        checked that the first spikes return at the same time.
        """
        begin = 5e-6

        # Configure mergers and DNC to output spikes to the dnc
        for merger in iter_all(C.DNCMergerOnHICANN):
            odd = bool(merger.value() % 2)
            m = self.h.layer1[merger]
            m.config = m.MERGE
            m.slow = False
            m.loopback = not odd

        for channel in iter_all(C.GbitLinkOnHICANN):
            if (channel.value()%2): # 1, 3, 5, 7
                self.h.layer1[channel] = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
            else:
                self.h.layer1[channel] = pyhalbe.HICANN.GbitLink.Direction.TO_HICANN

        spike_times = np.arange(no_spikes) * dt + begin
        runtime = spike_times[-1] + begin

        addr = pyhalbe.HICANN.L1Address(63)
        expected_spike_count = {}
        for ii in range(4):
            link = C.GbitLinkOnHICANN(ii*2)
            spikes = pysthal.Vector_Spike()
            for t in spike_times[ii::4]:
                spikes.append(pysthal.Spike(addr, t))
            expected_spike_count[C.GbitLinkOnHICANN((ii*2)+1)] = len(spikes)
            self.h.sendSpikes(link, spikes)

        # connect and configure
        self.run_experiment(0)

        err = ""

        # 1. Send and receive spike pattern serveral times
        first_spike_times = []
        for run in range(no_spike_runs):
            self.w.clearSpikes(received=True, send=False)
            self.start_runner(runtime)
            result = []
            local_drop_count = 0
            first_spikes = []
            for ii in [1,3,5,7]:
                link = C.GbitLinkOnHICANN(ii)
                received = self.h.receivedSpikes(C.GbitLinkOnHICANN(ii))
                times, addrs = received.T
                size = len(received)
                addrs = set(pyhalbe.HICANN.L1Address(int(addr)) for addr in addrs)
                result.append((link, size, addrs))
                first_spikes.extend((s[0], s[1], ii) for s in received)
            first_spikes.sort()
            if first_spikes:
                first_spike_times.append(first_spikes[0])

            for link, size, addrs in result:
                err_pre = "On %s, in run %i/%i: " % (link, run+1, no_spike_runs)

                if not drops_expected:
                    if size != expected_spike_count[link]:
                        err += "%s received %i spike(s) instead of %i\n" % \
                               (err_pre, size, expected_spike_count[link])
                else:
                    local_drop_count += expected_spike_count[link] - size

                if addrs and addrs != set([addr]):
                    tmp = ", ".join([str(a) for a in (addrs - set([addr]))])
                    err += "%s received invalid addresses: %s\n" % (err_pre, tmp)

            # For pulse drop count test, we compare the lost pulses counted by the test
            #  with the pulse drop count registered by the FPGA TX FIFO
            if drops_expected:
                err_pre = "On run %i/%i: " % (run+1, no_spike_runs)
                st = self.w.status()
                fpga_drop_counter = st.get_dropped_pulses_from_fpga(self.hicann_c.toFPGAOnWafer(),
                                                                    self.hicann_c.toHICANNOnDNC())
                if local_drop_count != fpga_drop_counter:
                    err += "%s Mismatch of observed pulse drops (%i) with drops counted by the FPGA (%i)\n" % \
                           (err_pre, local_drop_count, fpga_drop_counter)
                else:
                    print("Local pulse drop count of %i matches FPGA TX FIFO drop count" % (local_drop_count))

        if np.ptp([x[0] for x in first_spike_times]) > 4.0/FPGA_FREQ:
            tmp = [x[0] for x in first_spike_times]
            msg = "The start time of the spike trains variates to much: {} "
            msg += " (ptp: {} >= {})"
            err += msg.format(tmp, np.ptp(tmp), 4.0/FPGA_FREQ)

        # 2. Send no spikes to check, that no old spikes are hanging somewhere
        # and come back in the next experiment
        for run in range(no_empty_runs):
            self.w.clearSpikes(received=True, send=True)
            self.start_runner(runtime)
            received = sum(len(self.h.receivedSpikes(C.GbitLinkOnHICANN(ii)))
                           for ii in [1,3,5,7])
            if received > 0:
                err += ("Unexpectedly received {} spike(s) on the test run {}/{}, "
                        "that doesn't send spikes\n".format(received, run+1, no_empty_runs))
                for ii in [1,3,5,7]:
                    link = C.GbitLinkOnHICANN(ii)
                    spikes = self.h.receivedSpikes(link)
                    if len(spikes):
                        spike_list = ", ".join("(t={}, addr={})".format(s[0], s[1])
                                               for s in spikes)
                        err += "   Received on {}: {}\n".format(link, spike_list)
        self.assertFalse(err, err)

    @hardware
    def test_hicann_loopback(self):
        """
        Send 200 times 2000 spikes to the HICANN and read them via hicann loopback.
        """
        self.logger.INFO("Starting test_hicann_loopback")
        self.hicann_loopback_impl(2000, 200)

    @hardware
    def test_hicann_loopback_min(self):
        """
        Send 200 times 1 spike to the HICANN and read them via hicann loopback.
        """
        self.logger.INFO("Starting test_hicann_loopback_min")
        self.hicann_loopback_impl(1, 200)

    @hardware
    def test_hicann_loopback_medium(self):
        """
        Send 10 times 100000 spikes to the HICANN and read them via hicann loopback.
        """
        self.logger.INFO("Starting test_hicann_loopback_medium")
        self.hicann_loopback_impl(100000, 10)

    @hardware
    @expensive
    def test_hicann_loopback_extreme(self):
        """
        Send 1 times ~33M spikes to the HICANN and read them via hicann loopback.
        This are 512MB of spikes.
        Run with reduced ISI to avoid extreme long experiment durations.
        """
        self.logger.INFO("Starting test_hicann_loopback_extreme")
        no_spikes = 512 * 1024**2 / 16  # 512MiB, 16 bytes per event
        self.hicann_loopback_impl(no_spikes, 1, 1, 3e-7)

    @hardware
    def test_hicann_loopback_with_pulse_drops(self):
        """
        Send 512 pulses to the HICANN and read them via hicann loopback.
        Run with reduced ISI that will cause the FPGA to drop pulses.
        The pulse drops counted from the sthal test should match the FPGA's drop counter.
        """
        self.logger.INFO("Starting test_hicann_loopback_drop_counters_at_fpga_tx")
        no_spikes = 512
        self.hicann_loopback_impl(no_spikes, 1, 1, 3e-8, drops_expected=True)

    @hardware
    def test_hicann_loopback_without_empty_check(self):
        """
        Send 200 times 2000 spikes to the HICANN and read them via hicann loopback but
        don't check for spikes in "empty" experiments.
        """
        self.logger.INFO("Starting test_hicann_loopback_without_empty_check")
        self.hicann_loopback_impl(2000, 200, 0)

    @hardware
    def test_hicann_loopback_without_empty_check_min(self):
        """
        Send 200 times 1 spike to the HICANN and read them via hicann loopback but
        don't check for spikes in "empty" experiments.
        """
        self.logger.INFO("Starting test_hicann_loopback_without_empty_check_min")
        self.hicann_loopback_impl(1, 200, 0)

    @hardware
    def test_hicann_loopback_without_empty_check_medium(self):
        """
        Send 10 times 100000 spikes to the HICANN and read them via hicann loopback but
        don't check for spikes in "empty" experiments.
        """
        self.logger.INFO("Starting test_hicann_loopback_without_empty_check_medium")
        self.hicann_loopback_impl(100000, 10, 0)

    @hardware
    @expensive
    def test_hicann_loopback_without_empty_check_extreme(self):
        """
        Send 1 times ~33M spikes to the HICANN and read them via hicann loopback but
        don't check for spikes in "empty" experiments.
        This are 512MB of spikes.
        Run with reduced ISI to avoid extreme long experiment durations.
        """
        self.logger.INFO("Starting test_hicann_loopback_without_empty_check_extreme")
        no_spikes = 512 * 1024**2 / 16  # 512MiB, 16 bytes per event
        self.hicann_loopback_impl(no_spikes, 1, 0, 3e-7)

    @hardware
    def test_driver_locking(self):
        """This test ensures, that synapse driver locking is archived within 0.15ms (real time)
        after experiment start.

        This means that a spike train of about 3700 spikes, starting add 600e-9 s with 80e-9 s spike
        distance will lock a synapse driver.
        """

        self.logger.INFO("Starting test_driver_locking")
        self.increase_switch_limit(columns=2)

        no_spikes = 10000
        spikes_in = np.arange(no_spikes) * 80e-9 + 600e-9
        max_lock_time = 0.30e-3

        # Configure mergers and DNC to output spikes to the dnc
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.MERGE
            m.slow = True
            m.loopback = False

        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = pyhalbe.HICANN.GbitLink.Direction.TO_HICANN

        for fg_block in iter_all(C.FGBlockOnHICANN):
            self.h.floating_gates.setShared(fg_block, pyhalbe.HICANN.V_dllres, 800)

        addr = pyhalbe.HICANN.L1Address(0)
        spikes = pysthal.Vector_Spike()
        for t in spikes_in:
            spikes.append(pysthal.Spike(addr, t))

        duration  = 0.04

        self.h.sendSpikes(self.PREOUT_LINK, spikes)

        for driver in self.PREOUT_DRIVERS:
            self.config_driver(driver, addr.getDriverDecoderMask())
            self.route(self.PREOUT_OUTPUT, driver)

        # FOR DEBUG
        #np.save("test_driver_locking_spikes_in.npy", spikes_in)

        err = ""
        self.connect()
        for analog in iter_all(C.AnalogOnHICANN):
            self.h.analog.set_preout(analog)
            #self.h.analog.set_dll_voltage(analog)

            recorder = self.h.analogRecorder(analog)
            recorder.activateTrigger(duration)
            self.run_experiment(duration, pysthal.HICANNConfigurator())

            dt, trace = recorder.getTimestamp(), recorder.trace()
            recorder.freeHandle()

            ii = analog.value()
            np.save("test_driver_locking_trace_{}.npy".format(ii), trace)

            spikes_out = self.find_spikes_in_preout(dt, trace)
            np.save("test_driver_locking_spikes_out_{}.npy".format(ii), spikes_out)

            if len(spikes_out) == 0:
                err += "On {}: No spikes received\n".format(analog)
            elif spikes_out[0] >= max_lock_time:
                err += "On {}: Driver locking took to long, first spike received after {}s, not {}s\n".format(
                        analog, spikes_out[0], max_lock_time)

        self.assertFalse(err, '\n' + err)

    @hardware
    def test_reset_on_synapse_array(self):
        self.logger.INFO("Starting test_reset_on_synapse_array")
        self.w.commonFPGASettings().setSynapseArrayReset(True)

        resets = 2

        SynapseWeight = pyhalbe.HICANN.SynapseWeight
        SynapseDecoder = pyhalbe.HICANN.SynapseDecoder
        DriverDecoder = pyhalbe.HICANN.DriverDecoder

        # Initialize array with random data
        for row in iter_all(C.SynapseRowOnHICANN):
            weights = np.random.randint(0, SynapseWeight.max+1, 256)
            decoders = np.random.randint(0, SynapseDecoder.max+1, 256)
            self.h.synapses[row].weights[:] = [SynapseWeight(int(w)) for w in weights]
            self.h.synapses[row].decoders[:] = [SynapseDecoder(int(d)) for d in decoders]

        for driver in iter_all(C.SynapseDriverOnHICANN):
            self.h.synapses[driver][top].set_decoder(top, DriverDecoder(int(np.random.randint(0, DriverDecoder.max))))
            self.h.synapses[driver][top].set_decoder(bottom, DriverDecoder(int(np.random.randint(0, DriverDecoder.max))))

            self.h.synapses[driver][bottom].set_decoder(top, DriverDecoder(int(np.random.randint(0, DriverDecoder.max))))
            self.h.synapses[driver][bottom].set_decoder(bottom, DriverDecoder(int(np.random.randint(0, DriverDecoder.max))))

        self.run_experiment(0.0)

        class TestSynapses(pysthal.HICANNConfigurator):
            def config_fpga(self, x, y):
                pass

            def config(self, fpga, handle, data):
                self.d_err = {}
                self.dd_err = {}
                self.w_err = {}
                for row in iter_all(C.SynapseRowOnHICANN):
                    if row in PysthalTest.COORDINATE_BLACKLIST:
                        continue
                    synapse_controller = pyhalbe.HICANN.SynapseController(
                            data.synapse_controllers[row.toSynapseArrayOnHICANN()])
                    weights = pyhalbe.HICANN.get_weights_row(handle, synapse_controller, row)
                    if data.synapses[row].weights != weights:
                        self.w_err[row] = weights
                for driver in iter_all(C.SynapseDriverOnHICANN):
                    if driver in PysthalTest.COORDINATE_BLACKLIST:
                        continue
                    synapse_controller = pyhalbe.HICANN.SynapseController(
                            data.synapse_controllers[driver.toSynapseArrayOnHICANN()])
                    d_top, d_bot = pyhalbe.HICANN.get_decoder_double_row(handle,
                            synapse_controller, driver)
                    row_top = C.SynapseRowOnHICANN(driver, top)
                    row_bot = C.SynapseRowOnHICANN(driver, bottom)
                    if data.synapses[row_top].decoders != d_top:
                        self.d_err[row_top] = d_top
                    if data.synapses[row_bot].decoders != d_bot:
                        self.d_err[row_bot] = d_bot
                for driver in iter_all(C.SynapseDriverOnHICANN):
                    if driver in PysthalTest.COORDINATE_BLACKLIST:
                        continue
                    synapse_controller = pyhalbe.HICANN.SynapseController(
                                         data.synapse_controllers[driver.toSynapseArrayOnHICANN()])
                    d = pyhalbe.HICANN.get_synapse_driver(handle,
                                                          synapse_controller,
                                                          driver)
                    if data.synapses[driver] != d:
                        self.dd_err[driver] = d

        def to_str(row):
            return ", ".join("{:>2}".format(int(x)) for x in row)

        for ii in range(resets):
            cfg = RerunConfigurator()
            self.w.configure(cfg)

            tester = TestSynapses()
            self.w.configure(tester)
            err = ""
            for row, v in tester.d_err.items():
                w = self.h.synapses[row].weights
                err += "Weight missmatch in synapse row {}:\n".format(row)
                err += " programmed:" + to_str(w) + "\n"
                err += " read:     :" + to_str(v) + "\n"

            for row, v in tester.w_err.items():
                w = self.h.synapses[row].decoders
                err += "Decoder missmatch in synapse row {}:\n".format(row)
                err += " programmed:" + to_str(w) + "\n"
                err += " read:     :" + to_str(v) + "\n"

            for driver, v in tester.dd_err.items():
                d = self.h.synapses[driver]
                err += "Driver missmatch for driver {}:\n".format(driver)
                err += " programmed:" + d.__str__() + "\n"
                err += " read:     :" + v.__str__() + "\n"

            self.assertFalse(err, '\n' + err)

    @hardware
    def test_layer2_input(self):
        """Send spike from FPGA playback memory to the HICANN and evalutes the
        preout to verify that the spikes are sent correctly.
        Synapse driver locking is archived via HICANN background generators."""

        self.logger.INFO("Starting test_layer2_input")
        self.increase_switch_limit(columns=2)

        no_spikes = 20000

        for bg in iter_all(C.BackgroundGeneratorOnHICANN):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(400)
            generator.address(pyhalbe.HICANN.L1Address(0))

        # Configure mergers and DNC to output spikes to the dnc
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.MERGE
            m.slow = True
            m.loopback = False

        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = pyhalbe.HICANN.GbitLink.Direction.TO_HICANN

        # Send spikes with address 63 every 2us, starting at 20us
        spikes_in = np.arange(no_spikes) * 2e-6 + 20e-6
        addr = pyhalbe.HICANN.L1Address(63)
        spikes = pysthal.Vector_Spike()
        for t in spikes_in:
            spikes.append(pysthal.Spike(addr, t))

        duration  = spikes_in[-1] + 300e-6 # Adding some grace time
        self.h.sendSpikes(self.PREOUT_LINK, spikes)

        for driver in self.PREOUT_DRIVERS:
            self.config_driver(driver, addr.getDriverDecoderMask())
            self.route(self.PREOUT_OUTPUT, driver)
        self.h.analog.set_preout(C.AnalogOnHICANN(0))
        self.h.analog.set_preout(C.AnalogOnHICANN(1))

        self.connect()
        recorder = self.h.analogRecorder(C.AnalogOnHICANN(0))
        recorder.activateTrigger(duration)

        self.run_experiment(duration)

        dt, trace = recorder.getTimestamp(), recorder.trace()
        spikes_out = self.find_spikes_in_preout(dt, trace)

        # for DEBUG
        # np.save("test_layer2_input_raw.npy", trace)
        # np.save("test_layer2_input_spikes_in.npy", spikes_in)
        # np.save("test_layer2_input_spikes_out.npy", spikes_out)

        isi_spikes_in =  np.mean(spikes_in[1:]  - spikes_in[:-1])
        isi_spikes_out = np.mean(spikes_out[1:] - spikes_out[:-1])

        err = ""
        if not recorder.hasTriggered():
            err += "ADC trigger hast not triggered\n"
        if len(spikes_in) != len(spikes_out):
            err += "Sent {} spikes, but received {}\n".format(len(spikes_in), len(spikes_out))
        if not self.almostEqual(isi_spikes_in, isi_spikes_out, 0.0001):
            err += "Distance between sent spikes should be {}, not {}\n".format(
                    isi_spikes_in, isi_spikes_out)
        if err:
            err += "Raw Trace len: {}, mean: {}, std: {}, min: {}, max: {}".format(
                len(trace), np.mean(trace), np.std(trace), np.min(trace), np.max(trace))
        self.assertFalse(err, '\n' + err)

    @hardware
    def test_trigger_start_time(self):
        self.logger.INFO("Starting test_trigger_start_time")
        tries = 5
        no_spikes = 500

        self.increase_switch_limit(columns=2)

        for bg in iter_all(C.BackgroundGeneratorOnHICANN):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(800)
            generator.address(pyhalbe.HICANN.L1Address(0))

        # Configure mergers and DNC to output spikes to the dnc
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.MERGE
            m.slow = True
            m.loopback = False

        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = pyhalbe.HICANN.GbitLink.Direction.TO_HICANN

        spikes_in = np.array([ii * 2.0e-6 for ii in range(10, 10 + no_spikes)])
        addr = pyhalbe.HICANN.L1Address(63)
        spikes = pysthal.Vector_Spike()
        for t in spikes_in:
            spikes.append(pysthal.Spike(addr, t))
        duration = spikes_in[-1]
        duration += 50.0e-6 # Add some grace time

        self.h.sendSpikes(self.PREOUT_LINK, spikes)

        for driver in self.PREOUT_DRIVERS:
            self.config_driver(driver, addr.getDriverDecoderMask())
            self.route(self.PREOUT_OUTPUT, driver)
        self.h.analog.set_preout(C.AnalogOnHICANN(0))
        self.h.analog.set_preout(C.AnalogOnHICANN(1))

        self.connect()
        recorder = self.h.analogRecorder(C.AnalogOnHICANN(0))

        self.run_experiment(0.0)

        times = []
        for ii in range(tries):
            recorder.activateTrigger(duration * 2.0)
            self.run_experiment(duration, RerunConfigurator())

            dt, trace = recorder.getTimestamp(), recorder.trace()
            spikes_out = self.find_spikes_in_preout(dt, trace)

            # for DEBUG
            # np.save("layer2_input_in.npy", spikes_in)
            # np.save("layer2_input_out.npy", spikes_out)

            isi_spikes_in =  np.mean(spikes_in[1:]  - spikes_in[:-1])
            isi_spikes_out = np.mean(spikes_out[1:] - spikes_out[:-1])

            err = ""
            if not recorder.hasTriggered():
                err += "ADC trigger hast not triggered\n"
            if len(spikes_in) != len(spikes_out):
                err += "Sent {} spikes, but received {}\n".format(len(spikes_in), len(spikes_out))
            if not self.almostEqual(isi_spikes_in, isi_spikes_out, 0.0001):
                err += "Distance between sent spikes should be {}, not {}\n".format(
                        isi_spikes_in, isi_spikes_out)
            self.assertFalse(err, '\n' + err)

            times.append(spikes_out[0])

        # TODO the values below might need to be adjusted when the trigger works right
        fpga_clk_freq = 250e6
        fpga_clk_cycle = 1.0/fpga_clk_freq
        adc_clk_cycle = dt

        # Tolerances
        std_times = np.std(times)
        std_tolerance = 2*adc_clk_cycle
        expected_t = spikes_in[0]

        err = ""
        if std_times > std_tolerance: # Two dnc clock cycles std, is this to much? :)
            err += "The time of the first spike scatters to much: {} > {}\n".format(std_times, std_tolerance)
        for t in times:
            # We expect the first spike at 2us
            if not self.almostEqual(t, expected_t, 0.01):
                err += "The time of the first spike is not at the expected timepoint: {} (expected: {})\n".format(t, expected_t)
        self.assertFalse(err, '\n' + err)

    @hardware
    def test_wafer_status(self):
        """test if we recive a proper status from the wafer
        this could be improved.
        """
        self.logger.INFO("Starting test_wafer_status")
        self.connect()
        st = self.w.status()
        self.assertTrue(np.any(np.array(st.fpga_rev) != 0))
        self.assertEqual(1, len(st.hicanns))
        print(st)

    @hardware
    def test_adc_trigger_analog_0(self):
        self.logger.INFO("Starting test_adc_trigger_analog_0")
        self._test_adc_trigger_impl(C.AnalogOnHICANN(0))

    @hardware
    def test_adc_trigger_analog_1(self):
        self.logger.INFO("Starting test_adc_trigger_analog_1")
        self._test_adc_trigger_impl(C.AnalogOnHICANN(1))

    def _test_adc_trigger_impl(self, analog):
        """Checks the ADC trigger protocol is working."""

        self.logger.INFO("Starting _test_adc_trigger_impl")
        duration = 500e-6

        self.connect()
        recorder = self.h.analogRecorder(analog)
        recorder.activateTrigger(duration)

        if(recorder.hasTriggered()):
            err = "ADC has triggered prior to experiment start"
            self.assertFalse(err, '\n' + err)

        with self.assertRaises(RuntimeError):
            recorder.trace()

        duration  = 0.04

        cfg = NoneConfigurator()
        self.w.configure(cfg)
        runner = pysthal.ExperimentRunner(duration)
        self.w.start(runner)

        self.assertTrue(recorder.hasTriggered())
        try:
            trace = recorder.trace()
        except RuntimeError:
            self.fail("recoder.trace()  should not throw.")


    @unittest.skip("Because I want")
    @hardware
    def test_self_induced_neuron_spiking(self):
        """ TODO """

        self.logger.INFO("Starting test_self_induced_neuron_spiking")
        shared_p = pyhalbe.HICANN.shared_parameter
        neuron_p = pyhalbe.HICANN.neuron_parameter

        fg = self.h.floating_gates
        for neuron in iter_all(C.NeuronOnHICANN):
            fg.setNeuron(neuron, neuron_p.E_l,    550)
            fg.setNeuron(neuron, neuron_p.E_syni, 300)
            fg.setNeuron(neuron, neuron_p.E_synx, 300)
            fg.setNeuron(neuron, neuron_p.I_gl,   120)
            fg.setNeuron(neuron, neuron_p.V_t,    500)

        for block in iter_all(C.FGBlockOnHICANN):
            fg.setShared(block, shared_p.V_reset, 100)


        for neuron in iter_all(C.NeuronOnHICANN):
            n = self.h.neurons[neuron]
            n.activate_firing(True)
            n.enable_spl1_output(neuron.y().value() == 0)
            addr = neuron.x().value() % 32 + neuron.y().value() * 32
            n.address(pyhalbe.HICANN.L1Address(addr))

        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.RIGHT_ONLY
            m.slow = False
            m.loopback = False

        direction = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = direction

        self.run_experiment(0.0)

        traces = {}
        for x in range(256):
            a, b = [(C.NeuronOnHICANN(C.X(x), C.Y(ii)), C.AnalogOnHICANN(ii)) for ii in range(2)]
            self.h.enable_aout(*a)
            self.h.enable_aout(*b)
            self.switch_aout()
            for neuron, analog in (a, b):
                readout = self.h.analogRecorder(analog)
                readout.setRecordingTime(1.0e-3)
                dt, data = readout.getTimestamp(), readout.record()
                traces[neuron] = data
                readout.freeHandle()

        np.save("membranes.npy", traces)

        self.run_experiment(0.1, NoopConfigurator)

        # Collect data
        neuronspikes = dict((n, []) for n in iter_all(C.NeuronOnHICANN))
        for channel in iter_all(C.GbitLinkOnHICANN):
            received = self.h.receivedSpikes(channel)
            c = channel.value()
            for addr, time in received:
                if addr < 32:
                    n = c * 32 + addr
                else:
                    n = 256 + c * 32 + addr - 32
                neuron = C.NeuronOnHICANN(Enum(n))
                self.assertEqual(neuron.x().value() % 32 + neuron.y().value() * 32, addr)
                neuronspikes[neuron].append(time)

        # FOR DEBUG
        # data = { "periods" : periods, "spiketimes" : spiketimes }
        np.save("spikefreq.npy", neuronspikes)

    @hardware
    def test_spike_interval(self):
        """Record periodic spikes. Calculate interval between timestamps
        and make sure there are no big jumps."""

        self.logger.INFO("Starting test_spike_interval")
        # Configure mergers and DNC to output spikes to the DNC
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.MERGE # CK: m.RIGHT_ONLY
            m.slow = True # CK: False
            m.loopback = False

        direction = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = direction

        addr = pyhalbe.HICANN.L1Address(1)

        for ii, bg in enumerate(iter_all(C.BackgroundGeneratorOnHICANN)):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(500)
            #generator.seed(undefined)
            generator.address(addr)

        recording_time = 1e-4
        self.run_experiment(recording_time)

        # Collect data
        no_spikes = []
        spiketimes = []
        for ii, channel in enumerate(iter_all(C.GbitLinkOnHICANN)):
            received = self.h.receivedSpikes(channel)
            _, times = received.T
            no_spikes.append(len(received))
            spiketimes.append(times)

        # make sure that the interval between timestamps is much smaller than
        # the counter overflow value
        max_interval = 0.2*MAX_TIMESTAMP
        for ii in range(len(spiketimes)):
            diff = spiketimes[ii][1:] - spiketimes[ii][:-1]
            for interval in diff:
                self.assertLess(abs(interval), max_interval, "incorrect timestamp overflow calculation")

    @hardware
    @unittest.expectedFailure
    def test_no_gaps(self):
        """Record periodic spikes. Make sure there are no gaps.

        This test is expected to fail on HICANN versions <= 4, because the
        clock is not synchronized correctly, when passing timestamps in the
        HICANN-DNC-interface. This causes bitflips in the spike times.
        """

        self.logger.INFO("Starting test_no_gaps")
        bg_period = 500
        pll_freq = self.w.commonFPGASettings().getPLL()
        bg_freq = pll_freq/(bg_period + 1)
        rel_tol = 10.0/bg_period

        # Configure mergers and DNC to output spikes to the DNC
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.MERGE # CK: m.RIGHT_ONLY
            m.slow = True # CK: False
            m.loopback = False

        direction = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        for channel in iter_all(C.GbitLinkOnHICANN):
            self.h.layer1[channel] = direction

        addr = pyhalbe.HICANN.L1Address(1)

        for ii, bg in enumerate(iter_all(C.BackgroundGeneratorOnHICANN)):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(bg_period)
            #generator.seed(undefined)
            generator.address(addr)

        recording_time = 1e-4
        self.run_experiment(recording_time)

        # Collect data
        no_spikes = []
        spiketimes = []
        for ii, channel in enumerate(iter_all(C.GbitLinkOnHICANN)):
            received = self.h.receivedSpikes(channel)
            times, _ = received.T
            no_spikes.append(len(received))
            spiketimes.append(times)

        # expecting interval between spikes to be approximately BG period
        errs = []
        expected_interval = 1.0 / bg_freq
        # expected_interval = float(bg_period)*1e-8  # FIXME why this magic number?
        for channel, times in enumerate(spiketimes):
            times.sort()
            intervals = np.diff(times)
            wrong_intervals = np.logical_not(
                self.almostEqual(intervals, expected_interval, rel_tol))
            positions, = np.where(wrong_intervals)
            if positions.size > 0:
                errs.append("BG spikes are not periodic on channel {}:".format(
                    channel))
            for pos in positions:
                errs.append("\tinterval {:>4}: {}".format(pos, intervals[pos]))

        print(errs)
        if errs:
            self.fail("\n".join(errs))


    @unittest.skip("not fully implemented")
    @hardware
    def test_compare_adc(self):
        """Record L2 timestamps of spiking neuron. Compare with detected spikes from ADC trace."""
        self.logger.INFO("Starting test_compare_adc")
        self.assertTrue(False, "not implemented")

    @hardware
    def test_readout(self):
        # Configure mergers and DNC to output spikes to the DNC
        self.logger.INFO("Starting test_readout")
        for merger in iter_all(C.DNCMergerOnHICANN):
            m = self.h.layer1[merger]
            m.config = m.MERGE # CK: m.RIGHT_ONLY
            m.slow = True # CK: False
            m.loopback = False

        for ii, bg in enumerate(iter_all(C.BackgroundGeneratorOnHICANN)):
            generator = self.h.layer1[bg]
            generator.enable(True)
            generator.random(False)
            generator.period(ii * 320)
            #generator.seed(undefined)
            generator.address(pyhalbe.HICANN.L1Address((ii * 23) % 64))

        self.run_experiment(0.0)
        readout_wafer = pysthal.Wafer(self.WAFER)
        hicann_vector = C.Vector_HICANNOnWafer()
        hicann_vector.append(self.HICANN)
        readout_cfg = pysthal.HICANNReadoutConfigurator(readout_wafer, hicann_vector)
        self.w.configure(readout_cfg)

        print(readout_wafer)
        readout_hicann = readout_wafer[self.hicann_c]
        print(self.h)
        print(readout_hicann)

        attrs = ['floating_gates', 'analog', 'repeater', 'synapses', 'neurons',
                 'layer1', 'synapse_switches', 'crossbar_switches',
                 'current_stimuli']
        for attr in attrs:
            print('=' * 80)
            print("{:=^80}".format(" {} ".format(attr)))
            print('=' * 80)
            print(getattr(self.h, attr))
            print(getattr(readout_hicann, attr))

    @hardware
    def test_verification_readout(self):
        """
        Configures a HICANN and runs the VerifyConfigurator. The results are
        tested with randomized:
            - Synapse weights and decoder values
            - Crossbar switches
            - Synapse switches
        """
        from numpy.testing import assert_array_equal

        self.logger.INFO("Starting test_verification_readout")
        self.increase_switch_limit(columns=255, rows=255)
        self.w.commonFPGASettings().setSynapseArrayReset(True)

        def set_decoders_and_weights(rnd, synapses):
            "set synapse decoders and weights with random values"
            no_synapses = C.SynapseOnHICANN.enum_type.size
            weights = rnd.randint(0, 15+1, size=no_synapses)
            decoders = rnd.randint(0, 15+1, size=no_synapses)

            SynapseWeight = pyhalbe.HICANN.SynapseWeight
            SynapseDecoder = pyhalbe.HICANN.SynapseDecoder
            synapses = self.h.synapses
            for synapse in iter_all(C.SynapseOnHICANN):
                s = synapses[synapse]
                s.weight = SynapseWeight(int(weights[int(synapse.toEnum())]))
                s.decoder = SynapseDecoder(int(decoders[int(synapse.toEnum())]))

            # Check that the values are set correctly
            assert_array_equal(weights, numpy.array(
                [synapses[synapse].weight
                 for synapse in iter_all(C.SynapseOnHICANN)],
                dtype=numpy.uint8))
            assert_array_equal(decoders, numpy.array(
                [synapses[synapse].decoder
                 for synapse in iter_all(C.SynapseOnHICANN)],
                dtype=numpy.uint8))
            return decoders, weights

        def set_crossbar(rnd, cs):
            "set crossbar switches with random values"
            size = (2, C.HLineOnHICANN.size, 4)
            values = rnd.choice([True, False], size=size)
            cs = self.h.crossbar_switches
            for hline in iter_all(C.HLineOnHICANN):
                cs.set_row(hline, left, values[0][int(hline)])
                cs.set_row(hline, right, values[1][int(hline)])
            return values

        def set_synapse_switches(rnd, synapse_switches):
            "set synapse switches with random values"
            size = (C.SynapseSwitchRowOnHICANN.enum_type.size, 16)
            values = rnd.choice([True, False], size=size)
            for row in iter_all(C.SynapseSwitchRowOnHICANN):
                synapse_switches.set_row(row, values[int(row.toEnum())])
            return values

        def set_current_stimulus(rnd, current_stimuli):
            size = (4, 129)
            values = rnd.randint(0, 1023+1, size=size)
            pulselength = rnd.randint(0, 15+1, size=4)
            # Keep one unchanged in the second thes phase
            values[2][:] = list(range(321, 578, 2))
            pulselength[2] = 7
            for ii in range(4):
                current_stimuli[ii][:] = [int(val) for val in values[ii]]
                current_stimuli[ii].setPulselength(int(pulselength[ii]))
            return values, pulselength

        rnd = numpy.random.RandomState(seed=2310013)

        ########################
        # First test phase, we store random values in the sthal configuration
        # and check that they are correctly read back by VerifyConfigurator
        decoders, weights = set_decoders_and_weights(rnd, self.h.synapses)
        crossbar_switches = set_crossbar(rnd, self.h.crossbar_switches)
        synapse_switches = set_synapse_switches(rnd, self.h.synapse_switches)
        current_stimuli, pulselength = set_current_stimulus(
            rnd, self.h.current_stimuli)

        self.run_experiment(0.0)

        cfg = pysthal.VerifyConfigurator()
        self.w.configure(cfg)
        first_pass_configuration_errors = [
            r for r in cfg.results()
            if r.reliable and r.errors > 0]
        first_pass_error_count = cfg.error_count()

        ########################
        # Second test phase, we mess with the configuration stored in the
        # StHAL container and check that VerifyConfigurator detects this.
        # We check that the VerifyConfigurator finds the correct number of
        # errors
        decoders2, weights2 = set_decoders_and_weights(rnd, self.h.synapses)
        crossbar_switches2 = set_crossbar(rnd, self.h.crossbar_switches)
        synapse_switches2 = set_synapse_switches(rnd, self.h.synapse_switches)
        current_stimuli2, pulselength2 = set_current_stimulus(rnd, self.h.current_stimuli)

        cfg = pysthal.VerifyConfigurator()  # Just read back, don't update values on hardware
        self.w.configure(cfg)

        # Synapse Rows 220 till 227 are missing on HICANNv4 and will not read
        # by the VerifyConfigurator, but instead always marked as correct
        # We correct the random weights and decoders to be equeal for this rows
        if self.HICANN_VERSION == 4:
            synidx = slice(220 * 256, 228 * 256)
            weights2[synidx] = weights[synidx]
            decoders2[synidx] = decoders[synidx]

        # Now count the number of errors we excpect for each part of the HICANN
        # configuration
        expected_error_counts = {
            "synapse_decoders": numpy.count_nonzero(decoders != decoders2),
            "synapse_weights": numpy.count_nonzero(weights != weights2),
            "l1_crossbar_switches": numpy.count_nonzero(
                crossbar_switches != crossbar_switches2),
            "l1_synapse_switches": numpy.count_nonzero(
                synapse_switches != synapse_switches2),
            "fg_config": numpy.count_nonzero(pulselength != pulselength2),
            "current_stimulus": + numpy.count_nonzero(
                numpy.all(current_stimuli != current_stimuli2, axis=1)),
        }

        ####################
        # Evaluate results and format error messages
        errors = []
        if first_pass_configuration_errors:
            errors.append("Unexpected configurations errors on HICANN:")
            for err in first_pass_configuration_errors:
                errors.append("   " + err.msg)
        if first_pass_error_count != 0:
            errors.append("Configuration errors did occure")
        for result in cfg.results():
            expected = expected_error_counts.get(result.subsystem, 0)
            if result.errors != expected and result.reliable:
                errors.append(
                    "{}: Found {}, expected {} configuration errors".format(
                        result.subsystem, result.errors, expected))
        if cfg.error_count() != sum(expected_error_counts.values()):
            errors.append("Invalid total error count {}, expected {}".format(
                cfg.error_count(), sum(expected_error_counts.values())))

        if errors:
            self.fail("\n".join(errors))


if __name__ == '__main__':
    PysthalTest.main()
