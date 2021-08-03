#!/usr/bin/env python

from PysthalTest import PysthalTest, hardware
import unittest

import numpy as np
import pysthal
import pyhalbe
import pylogging
from pyhalco_common import Enum
import pyhalco_hicann_v2 as Coord

class TestMultiHICANN(PysthalTest):

    def setUp(self):
        super(TestMultiHICANN, self).setUp()

        pylogging.set_loglevel(pylogging.get("Default"), pylogging.LogLevel.ERROR)
        pylogging.set_loglevel(pylogging.get("sthal"), pylogging.LogLevel.INFO)

        if None in (self.WAFER, self.HICANN):
            return

        self.wafer_c = Coord.Wafer(self.WAFER)

        self.w = pysthal.Wafer(self.wafer_c)
        self.h1 = self.w[Coord.HICANNOnWafer(Enum(324))]
        self.h2 = self.w[Coord.HICANNOnWafer(Enum(120))]

        self.addCleanup(self.w.disconnect)

    def connect(self):
        self.w.connect(pysthal.MagicHardwareDatabase())

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

    @hardware
    def test_wafer_status(self):
        """test if we receive a proper status from the wafer
        this could be improved.
        """
        self.connect()
        self.w.configure(pysthal.JustResetHICANNConfigurator())
        st = self.w.status()
        self.assertTrue(np.any(np.array(st.fpga_rev) != 0))
        self.assertEqual(2, len(st.hicanns))
        print(st)

if __name__ == '__main__':
    PysthalTest.main()
