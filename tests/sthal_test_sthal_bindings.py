#!/usr/bin/env python

# Make tests fail, if there are warnings on module import
# import warnings
# with warnings.catch_warnings(record=True) as w:
#     warnings.filterwarnings('error')
#     import pysthal

from HWTest import PyhalbeTest

class Test_PyhalbeBindings(PyhalbeTest):
    def test_SynapseProxies(self):
        import pysthal
        import pyhalbe
        import pyhalco_hicann_v2
        C = pyhalco_hicann_v2

        hicann = pysthal.HICANN()
        row_c = C.SynapseRowOnHICANN(40)

        w = hicann.synapses[row_c].weights
        w[0] = pyhalbe.HICANN.SynapseWeight(7)

        self.assertEqual(hicann.synapses[row_c].weights[0], pyhalbe.HICANN.SynapseWeight(7))
        print hicann.synapses[row_c].weights[0]


    def test_synapse_access(self):
        import pysthal
        import numpy
        import pyhalco_common
        import pyhalco_hicann_v2
        from pyhalbe import HICANN

        d_patterns = {}
        w_patterns = {}

        hicann = pysthal.HICANN()
        for row in pyhalco_common.iter_all(pyhalco_hicann_v2.SynapseRowOnHICANN):
            d_pattern = numpy.random.randint(0, 16, 256)
            d_patterns[row] = d_pattern
            hicann.synapses[row].decoders[:] = [HICANN.SynapseDecoder(int(ii)) for ii in d_pattern]

            w_pattern = [HICANN.SynapseWeight(int(ii)) for ii in numpy.random.randint(0, 16, 256)]
            w_patterns[row] = w_pattern
            hicann.synapses[row].weights[:] = w_pattern

        for drv in pyhalco_common.iter_all(pyhalco_hicann_v2.SynapseDriverOnHICANN):
            double_row = hicann.synapses.getDecoderDoubleRow(drv)
            for x in (pyhalco_common.top, pyhalco_common.bottom):
                row = pyhalco_hicann_v2.SynapseRowOnHICANN(drv, x)
                data = numpy.array([int(ii) for ii in double_row[x.value()]])
                if not numpy.all(data == d_patterns[row]):
                    err = "Missmatch in decoder values: {!s} != {!s} in {!s}".format(data, d_patterns[row], row)
                    self.fail(err)

        for syn in pyhalco_common.iter_all(pyhalco_hicann_v2.SynapseOnHICANN):
            hicann.synapses[syn]

    def test_common_FPGA_config(self):
        import pysthal
        import pyhalco_hicann_v2
        w = pysthal.Wafer(pyhalco_hicann_v2.Wafer())
        s1 = w.commonFPGASettings()
        s2 = w.commonFPGASettings()
        s1.setPLL(200.0e6)
        self.assertEqual(s1.getPLL(), s2.getPLL())
        s3 = w.commonFPGASettings()
        self.assertEqual(s1.getPLL(), s3.getPLL())

        for pll in (100.0e6, 150.0e6, 200.0e6, 250.0e6):
            s1.setPLL(pll)

        for pll in (53.0e6, 32.0e6, 430.e6, 170.e6):
            self.assertRaises(ValueError, s1.setPLL, pll)


    def test_numpy_policies(self):
        import numpy
        import pysthal
        import pyhalco_hicann_v2
        from pyhalbe import HICANN
        w = pysthal.Wafer(pyhalco_hicann_v2.Wafer())
        h = w[pyhalco_hicann_v2.HICANNOnWafer()]

        addrs = numpy.array(numpy.random.randint(64, size=100), dtype=numpy.ushort)
        times = numpy.cumsum(numpy.random.poisson(10.0, size=100)) * 1.e-6
        in_spikes = pysthal.Vector_Spike()
        for addr, t in zip(addrs, times):
            in_spikes.append(pysthal.Spike(HICANN.L1Address(addr), t))

        link = pyhalco_hicann_v2.GbitLinkOnHICANN(3)
        h.sendSpikes(link, in_spikes)
        h.sortSpikes()
        x = h.sentSpikes(link)
        times_t, addrs_t = x.T

        numpy.testing.assert_allclose(times, times_t, rtol=0.0, atol=1.0/250e6)
        numpy.testing.assert_array_equal(addrs, numpy.array(addrs_t, dtype=numpy.ushort))


    def test_AnalogRecorder(self):
        import pysthal
        self.assertIs(pysthal.AnalogRecorder.voltage_type, float)
        self.assertIs(pysthal.AnalogRecorder.time_type, float)

    def test_FGConfig_serialization(self):
        """
        Verifies that all members of sthal::FGConfig are serialized/pickled, not only the ones from
        halbe::FGConfig, see  #2124
        """
        import pysthal
        from copy import deepcopy

        a = pysthal.FGConfig()
        a.writeDown = True
        a.fg_biasn = 2
        b = deepcopy(a)
        self.assertEqual(a.fg_biasn, b.fg_biasn)
        self.assertEqual(a.writeDown, b.writeDown)
        self.assertEqual(a, b)

    def test_manual_type_renaming(self):
        import pysthal
        self.assertNotEqual(getattr(pysthal, 'VectorHICANNHandle', False), False)
        self.assertNotEqual(getattr(pysthal, 'VectorHICANNData', False), False)

if __name__ == '__main__':
    Test_PyhalbeBindings.main()
