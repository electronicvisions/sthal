#!/usr/bin/env python

from HWTest import PyhalbeTest
import numpy
import tempfile
from collections import defaultdict

class TestSthalAlgorithms(PyhalbeTest):
    def setUp(self):
        import pylogging
        pylogging.default_config(pylogging.LogLevel.FATAL)

    def test_import(self):
        import pysthal

    def test_neuronsize(self):
        import pysthal
        from pyhalco_common import iter_all, X
        from pyhalco_hicann_v2 import NeuronOnHICANN, QuadOnHICANN
        hicann = pysthal.HICANN()
        sizes = [2**x for x in range(7)]
        # There and back again
        for size in sizes[:] + sizes[::-1]:
            hicann.set_neuron_size(size)
            hicann.check()
            active = [hicann.neurons[neuron].activate_firing()
                    for neuron in iter_all(NeuronOnHICANN)]
            self.assertEqual(active.count(True), 512/size)
            for quad in iter_all(QuadOnHICANN):
                self.assertEqual(hicann.neurons[quad].getVerticalInterconnect(
                        X(0)), (size > 1))
                self.assertEqual(hicann.neurons[quad].getVerticalInterconnect(
                        X(1)), (size > 1))

    def test_save_and_load(self):
        import pysthal
        from pyhalco_common import Enum
        import pyhalco_hicann_v2 as Coordinate
        from pyhalbe import HICANN
        wafer = pysthal.Wafer(Coordinate.Wafer(3))
        hicann1 = wafer[Coordinate.HICANNOnWafer(Enum(30))]

        for row in Coordinate.iter_all(Coordinate.SynapseRowOnHICANN):
            d_pattern = numpy.random.randint(0, 16, 256)
            d_pattern[d_pattern[0] + 23] = 15
            hicann1.synapses[row].decoders[:] = [HICANN.SynapseDecoder(int(ii)) for ii in d_pattern]
            w_pattern = numpy.random.randint(0, 16, 256)
            w_pattern[w_pattern[0] + 23] = 15
            hicann1.synapses[row].weights[:] = [HICANN.SynapseWeight(int(ii)) for ii in w_pattern]

        wafer2 = pysthal.Wafer(Coordinate.Wafer(0))
        hicann2 = wafer2[Coordinate.HICANNOnWafer(Enum(42))]
        self.assertNotEqual(str(wafer.status()), str(wafer2.status()))

        for row in Coordinate.iter_all(Coordinate.SynapseRowOnHICANN):
            d1 = hicann1.synapses[row].decoders
            d2 = hicann2.synapses[row].decoders
            self.assertNotEqual(d1, d2)

            w1 = hicann1.synapses[row].weights
            w2 = hicann2.synapses[row].weights
            self.assertNotEqual(w1, w2)

        with tempfile.NamedTemporaryFile() as f:
            wafer.dump(f.name, True)
            wafer2.load(f.name)

        self.assertEqual(wafer.size(), wafer2.size())
        hicann1 = wafer[Coordinate.HICANNOnWafer(Enum(30))]
        hicann2 = wafer2[Coordinate.HICANNOnWafer(Enum(30))]
        self.assertEqual(hicann1.index(), hicann2.index())
        self.assertEqual(str(wafer.status()), str(wafer2.status()))

        for row in Coordinate.iter_all(Coordinate.SynapseRowOnHICANN):
            d1 = hicann1.synapses[row].decoders
            d2 = hicann2.synapses[row].decoders
            self.assertEqual(d1, d2)

            w1 = hicann1.synapses[row].weights
            w2 = hicann2.synapses[row].weights
            self.assertEqual(w1, w2)

    def test_check_testports(self):
        """
        Check if checking for more than one test output per test port
        per repeater block works.
        """
        import pyhalbe
        import pysthal
        from pyhalco_common import Enum, left, top
        import pyhalco_hicann_v2 as C

        hicann = pysthal.HICANN()

        # HRepeaterOnHICANN 0 and 2 are on different test ports
        hicann.repeater[C.HRepeaterOnHICANN(Enum(0))].setOutput(left)
        hicann.repeater[C.HRepeaterOnHICANN(Enum(2))].setOutput(left)
        self.assertEqual(hicann.check(), "")
        hicann.repeater.clearReapeater()

        # HRepeaterOnHICANN 0 and 4 are on the same test port
        hicann.repeater[C.HRepeaterOnHICANN(Enum(0))].setOutput(left)
        hicann.repeater[C.HRepeaterOnHICANN(Enum(4))].setOutput(left)
        self.assertNotEqual(hicann.check(), "")
        hicann.repeater.clearReapeater()

        # VRepeaterOnHICANN 0 and 2 are on the same test port
        hicann.repeater[C.VRepeaterOnHICANN(Enum(0))].setOutput(top)
        hicann.repeater[C.VRepeaterOnHICANN(Enum(2))].setOutput(top)
        self.assertNotEqual(hicann.check(), "")
        hicann.repeater.clearReapeater()

        # VRepeaterOnHICANN 0 and 4 are on different test ports
        hicann.repeater[C.VRepeaterOnHICANN(Enum(0))].setOutput(top)
        hicann.repeater[C.VRepeaterOnHICANN(Enum(1))].setOutput(top)
        self.assertEqual(hicann.check(), "")
        hicann.repeater.clearReapeater()

    def test_crossbar_exclusiveness(self):
        """
        Check if checking of exclusiveness of the crossbar switch matrix works.

        Warning: Does not check for all possible valid and invalid combinations.
        """

        import pysthal
        import pyhalco_hicann_v2 as C

        hicann = pysthal.HICANN()
        settings = pysthal.Settings.get()

        # VLineOnHICANN::toHLineOnHICANN not implemented
        vline_to_hline = defaultdict(list)

        for hline in C.iter_all(C.HLineOnHICANN):
            for vline1, vline2 in zip(hline.toVLineOnHICANN(),
                                      hline.toVLineOnHICANN()[1:]):

                vline_to_hline[vline1].append(hline)

                settings.crossbar_switches.max_switches_per_row = 1

                # one switch in hline
                hicann.crossbar_switches.set(vline1, hline, True)
                self.assertEqual(hicann.check(), "")

                # two switches in hline
                hicann.crossbar_switches.set(vline2, hline, True)
                self.assertNotEqual(hicann.check(), "")
                settings.crossbar_switches.max_switches_per_row = 2
                self.assertEqual(hicann.check(), "")

                hicann.clear_l1_switches()

        for vline, hlines in vline_to_hline.iteritems():
            for hline1, hline2 in zip(hlines, hlines[1:]):

                settings.crossbar_switches.max_switches_per_column = 1

                # one switch in vline
                hicann.crossbar_switches.set(vline, hline1, True)
                self.assertEqual(hicann.check(), "")

                # two switches in vline
                hicann.crossbar_switches.set(vline, hline2, True)
                self.assertNotEqual(hicann.check(), "")
                settings.crossbar_switches.max_switches_per_column = 2
                self.assertEqual(hicann.check(), "")

                hicann.clear_l1_switches()

    def test_synapseswitch_exclusiveness(self):
        """
        Check if checking of exclusiveness of synapse switch matrix works.

        Warning: Does not check for all possible valid and invalid combinations.
        """

        from pyhalbe.HICANN import SynapseSwitch
        import pysthal
        from pyhalco_common import iter_all, SideHorizontal
        import pyhalco_hicann_v2 as C

        hicann = pysthal.HICANN()
        settings = pysthal.Settings.get()

        # check per vline
        for side in iter_all(SideHorizontal):
            for vline in iter_all(C.VLineOnHICANN):
                syn_drvs = [syn_drv for syn_drv in vline.toSynapseDriverOnHICANN(side)]
                for syn_drv1, syn_drv2 in zip(syn_drvs, syn_drvs[1:]):

                    settings.synapse_switches.max_switches_per_column_per_side = 1

                    ssr1 = syn_drv1.toSynapseSwitchRowOnHICANN().line()
                    ssr2 = syn_drv2.toSynapseSwitchRowOnHICANN().line()

                    # one switch in vline -> ok
                    hicann.synapse_switches.set(vline, ssr1, True)
                    self.assertEqual(hicann.check(), "")

                    # two switches in vline -> invalid
                    hicann.synapse_switches.set(vline, ssr2, True)
                    self.assertNotEqual(hicann.check(), "")
                    settings.synapse_switches.max_switches_per_column_per_side = 2
                    self.assertEqual(hicann.check(), "")
                    hicann.clear_l1_switches()


        # check per synapse switch row
        for syn_drv in C.iter_all(C.SynapseDriverOnHICANN):
            ssr = syn_drv.toSynapseSwitchRowOnHICANN()
            vlines = [vl for vl in SynapseSwitch.get_lines(ssr)]
            for vline1, vline2 in zip(vlines, vlines[1:]):

                settings.synapse_switches.max_switches_per_row = 1

                # one switch in switch row -> ok
                hicann.synapse_switches.set(vline1, ssr.line(), True)
                self.assertEqual(hicann.check(), "")

                # two switches in switch row -> invalid
                hicann.synapse_switches.set(vline2, ssr.line(), True)
                self.assertNotEqual(hicann.check(), "")
                settings.synapse_switches.max_switches_per_row = 2
                self.assertEqual(hicann.check(), "")

                hicann.clear_l1_switches()

    def test_find_neuron_in_analog_output(self):
        """
        Check if find_neuron_in_analog_output works.
        """
        import pysthal
        from pyhalco_common import iter_all, X, Y
        from pyhalco_hicann_v2 import NeuronOnHICANN, AnalogOnHICANN
        hicann = pysthal.HICANN()

        NeuronOnHICANN.__repr__ = NeuronOnHICANN.__str__

        # Nothing active, this should throw
        for analog in iter_all(AnalogOnHICANN):
            with self.assertRaises(RuntimeError):
                hicann.find_neuron_in_analog_output(analog)

        # Enable 4 neurons, connected to different analog mux entries, but not
        # the mux yet, this should throw
        hicann.neurons[NeuronOnHICANN(X(0), Y(0))].enable_aout(True)
        hicann.neurons[NeuronOnHICANN(X(1), Y(0))].enable_aout(True)
        hicann.neurons[NeuronOnHICANN(X(0), Y(1))].enable_aout(True)
        hicann.neurons[NeuronOnHICANN(X(1), Y(1))].enable_aout(True)
        for analog in iter_all(AnalogOnHICANN):
            with self.assertRaises(RuntimeError):
                hicann.find_neuron_in_analog_output(analog)

        # Now enable the mux for each of the neurons and check if the correct
        # neuron was found
        for analog in iter_all(AnalogOnHICANN):
            hicann.analog.set_membrane_top_even(analog)
            self.assertEqual(NeuronOnHICANN(X(0), Y(0)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.set_membrane_top_odd(analog)
            self.assertEqual(NeuronOnHICANN(X(1), Y(0)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.set_membrane_bot_even(analog)
            self.assertEqual(NeuronOnHICANN(X(0), Y(1)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.set_membrane_bot_odd(analog)
            self.assertEqual(NeuronOnHICANN(X(1), Y(1)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.disable(analog)

        # Now enable 4 more, each connected to different analog mux entries,
        # now it should fail, because two neurons are connected
        hicann.neurons[NeuronOnHICANN(X(30), Y(0))].enable_aout(True)
        hicann.neurons[NeuronOnHICANN(X(41), Y(0))].enable_aout(True)
        hicann.neurons[NeuronOnHICANN(X(10), Y(1))].enable_aout(True)
        hicann.neurons[NeuronOnHICANN(X(255), Y(1))].enable_aout(True)

        for analog in iter_all(AnalogOnHICANN):
            hicann.analog.set_membrane_top_even(analog)
            with self.assertRaises(RuntimeError):
                hicann.find_neuron_in_analog_output(analog)
            hicann.analog.set_membrane_top_odd(analog)
            with self.assertRaises(RuntimeError):
                hicann.find_neuron_in_analog_output(analog)
            hicann.analog.set_membrane_bot_even(analog)
            with self.assertRaises(RuntimeError):
                hicann.find_neuron_in_analog_output(analog)
            hicann.analog.set_membrane_bot_odd(analog)
            with self.assertRaises(RuntimeError):
                hicann.find_neuron_in_analog_output(analog)
            hicann.analog.disable(analog)

        # Now disable the first 4 neurons, and everything should be fine again
        hicann.neurons[NeuronOnHICANN(X(0), Y(0))].enable_aout(False)
        hicann.neurons[NeuronOnHICANN(X(1), Y(0))].enable_aout(False)
        hicann.neurons[NeuronOnHICANN(X(0), Y(1))].enable_aout(False)
        hicann.neurons[NeuronOnHICANN(X(1), Y(1))].enable_aout(False)
        for analog in iter_all(AnalogOnHICANN):
            hicann.analog.set_membrane_top_even(analog)
            self.assertEqual(NeuronOnHICANN(X(30), Y(0)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.set_membrane_top_odd(analog)
            self.assertEqual(NeuronOnHICANN(X(41), Y(0)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.set_membrane_bot_even(analog)
            self.assertEqual(NeuronOnHICANN(X(10), Y(1)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.set_membrane_bot_odd(analog)
            self.assertEqual(NeuronOnHICANN(X(255), Y(1)),
                             hicann.find_neuron_in_analog_output(analog))
            hicann.analog.disable(analog)

    def test_has_outbound_mergers(self):
        import pyhalbe
        import pysthal
        from pyhalco_common import Enum
        import pyhalco_hicann_v2 as C

        wafer_c = C.Wafer(33)
        gbitlink_c = C.GbitLinkOnHICANN(Enum(0))
        fpga_on_wafer_c = C.FPGAOnWafer(Enum(0))
        fpga_c = C.FPGAGlobal(fpga_on_wafer_c, wafer_c)
        hicann_cs = [C.HICANNGlobal(h, wafer_c) for h in fpga_c.toHICANNOnWafer()]
        hicann_c = hicann_cs[0]
        hicann_on_dnc_c = hicann_c.toHICANNOnWafer().toHICANNOnDNC()
        dnc_on_fpga_c = hicann_c.toDNCOnFPGA()

        w = pysthal.Wafer()
        h = w[hicann_c.toHICANNOnWafer()]
        f = w[fpga_on_wafer_c]

        self.assertFalse(f.hasOutboundMergers())

        f[dnc_on_fpga_c][hicann_on_dnc_c].layer1[gbitlink_c] = pyhalbe.HICANN.GbitLink.Direction.TO_DNC
        self.assertTrue(f.hasOutboundMergers())

    def test_get_address(self):
        """
        Tests getting the full 6-bit L1 address a synapse decodes
        """

        import pyhalbe
        import pysthal
        from pyhalco_common import Enum, top, bottom
        import pyhalco_hicann_v2 as C

        h = pysthal.HICANN()

        for syn_c in C.iter_all(C.SynapseOnHICANN):

            drv_c = syn_c.toSynapseDriverOnHICANN()
            row_config_c = syn_c.toSynapseRowOnHICANN().toRowOnSynapseDriver()

            syn = h.synapses[syn_c]
            drv = h.synapses[drv_c]
            row_config = drv[row_config_c]

            for addr in C.iter_all(pyhalbe.HICANN.L1Address):
                syn.decoder = addr.getSynapseDecoderMask()

                row_config.set_decoder(top if syn_c.toSynapseColumnOnHICANN().toEnum().value() % 2 == 0 else bottom,
                                       addr.getDriverDecoderMask())

                self.assertEqual(addr, h.synapses.get_address(syn_c))

if __name__ == '__main__':
    TestSthalAlgorithms.main()
