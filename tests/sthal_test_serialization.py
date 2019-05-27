#!/usr/bin/env python
import unittest
import pysthal
import Coordinate as C

class TestSerialization(unittest.TestCase):
    def test_wafer_pickle(self):
        import pickle
        wafer_c = C.Wafer(33)
        w = pysthal.Wafer(wafer_c)
        h_c = C.HICANNOnWafer(C.Enum(297))
        h = w[h_c]
        self.assertTrue(h.has_wafer())
        w_str = pickle.dumps(w)
        w2 = pickle.loads(w_str)
        self.assertEqual(w, w2)
        h2 = w2[h_c]
        self.assertTrue(h2.has_wafer())
        # change something to ensure that it's not a mere pointer copy
        h.set_neuron_size(2)
        self.assertNotEqual(w, w2)

    def test_wafer_dumpnload(self):
        import tempfile
        wafer_c = C.Wafer(33)
        w = pysthal.Wafer(wafer_c)
        h_c = C.HICANNOnWafer(C.Enum(297))
        h = w[h_c]
        with tempfile.NamedTemporaryFile() as f:
            w.dump(f.name, True)
            w2 = pysthal.Wafer(wafer_c)
            w2.load(f.name)
        self.assertEqual(w,w2)
        h2 = w2[h_c]
        self.assertTrue(h2.has_wafer())
        # change something to ensure that it's not a mere pointer copy
        h.set_neuron_size(2)
        self.assertNotEqual(w, w2)

    def test_wafer_copy(self):
        import copy
        wafer_c = C.Wafer(33)
        w = pysthal.Wafer(wafer_c)
        h_c = C.HICANNOnWafer(C.Enum(297))
        h = w[h_c]
        self.assertTrue(h.has_wafer())
        w2 = copy.deepcopy(w)
        self.assertEqual(w, w2)
        h2 = w2[h_c]
        self.assertTrue(h2.has_wafer())
        # change something to ensure that it's not a mere pointer copy
        h.set_neuron_size(2)
        self.assertNotEqual(w, w2)

    def test_hicann_pickle(self):
        import pickle
        h = pysthal.HICANN()
        self.assertFalse(h.has_wafer())
        h_str = pickle.dumps(h)
        h2 = pickle.loads(h_str)
        self.assertEqual(h, h2)
        # change something to ensure that it's not a mere pointer copy
        h.set_neuron_size(2)
        self.assertNotEqual(h, h2)

    def test_hicann_copy(self):
        import copy
        h = pysthal.HICANN()
        self.assertFalse(h.has_wafer())
        h2 = copy.deepcopy(h)
        self.assertEqual(h, h2)
        # change something to ensure that it's not a mere pointer copy
        h.set_neuron_size(2)
        self.assertNotEqual(h, h2)

    def test_dnc_pickle(self):
        import pickle
        wafer_c = C.Wafer(33)
        w = pysthal.Wafer(wafer_c)
        h_c = C.HICANNGlobal(C.HICANNOnWafer(C.Enum(297)), wafer_c)
        h = w[h_c.toHICANNOnWafer()]
        d = w[h_c.toFPGAOnWafer()][h_c.toDNCOnFPGA()]
        d_str = pickle.dumps(d)
        d2 = pickle.loads(d_str)
        self.assertEqual(d, d2)
        # change something to ensure that it's not a mere pointer copy
        h2 = w[C.HICANNOnWafer(C.Enum(298))]
        self.assertNotEqual(d, d2)

    def test_dnc_copy(self):
        import copy
        wafer_c = C.Wafer(33)
        w = pysthal.Wafer(wafer_c)
        h_c = C.HICANNGlobal(C.HICANNOnWafer(C.Enum(297)), wafer_c)
        h = w[h_c.toHICANNOnWafer()]
        d = w[h_c.toFPGAOnWafer()][h_c.toDNCOnFPGA()]
        d2 = copy.deepcopy(d)
        self.assertEqual(d, d2)
        # change something to ensure that it's not a mere pointer copy
        h2 = w[C.HICANNOnWafer(C.Enum(298))]
        self.assertNotEqual(d, d2)

    def test_fpga_pickle(self):
        import pickle
        import pyhalbe
        f = pysthal.FPGA(C.FPGAGlobal(), pysthal.FPGAShared())
        f_str = pickle.dumps(f)
        f2 = pickle.loads(f_str)
        self.assertEqual(f, f2)
        # change something to ensure that it's not a mere pointer copy
        f.insertReceivedPulseEvent(pyhalbe.FPGA.PulseEvent())
        self.assertNotEqual(f, f2)

    def test_fpga_copy(self):
        import copy
        import pyhalbe
        f = pysthal.FPGA(C.FPGAGlobal(), pysthal.FPGAShared())
        f2 = copy.deepcopy(f)
        self.assertEqual(f, f2)
        # change something to ensure that it's not a mere pointer copy
        f.insertReceivedPulseEvent(pyhalbe.FPGA.PulseEvent())
        self.assertNotEqual(f, f2)

if __name__ == '__main__':
    unittest.main()
