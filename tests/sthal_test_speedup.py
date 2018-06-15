#!/usr/bin/env python

import unittest

import Coordinate
import pysthal


class TestSthalSpeedup(unittest.TestCase):
    def setUp(self):
        """Create HICANN object"""
        c_hicann = Coordinate.HICANNOnWafer(Coordinate.Enum(0))
        wafer = pysthal.Wafer()
        self.hicann = wafer[c_hicann]
        self.options = [
            pysthal.NORMAL,
            pysthal.FAST,
            pysthal.SLOW
        ]

    def test_speedup_gl(self):
        """
        Test setting and getting speedup.
        """
        h = self.hicann
        for speedup in self.options:
            h.set_speed_up_gl(speedup)
            self.assertTrue(h.get_speed_up_gl() is speedup)

    def test_speedup_gladapt(self):
        """
        Test setting and getting speedup.
        """
        h = self.hicann
        for speedup in self.options:
            h.set_speed_up_gladapt(speedup)
            self.assertTrue(h.get_speed_up_gladapt() is speedup)

    def test_speedup_radapt(self):
        """
        Test setting and getting speedup.
        """
        h = self.hicann
        for speedup in self.options:
            h.set_speed_up_radapt(speedup)
            self.assertTrue(h.get_speed_up_radapt() is speedup)


if __name__ == '__main__':
    unittest.main()
