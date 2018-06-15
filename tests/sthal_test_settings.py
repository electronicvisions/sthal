#!/usr/bin/env python

import unittest
import pysthal


class TestSthalSettings(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """Settings is singleton, save defaults in setUp"""
        settings = pysthal.Settings.get()
        cls.default_calibtic_backend = str(settings.calibtic_backend)
        cls.default_calibtic_host = str(settings.calibtic_host)

    def test_defaults(self):
        # default setting should be xml
        self.assertEqual(self.default_calibtic_backend, "xml")
        self.assertNotEqual(self.default_calibtic_host, "cetares")

    def test_xml(self):
        settings = pysthal.Settings.get()
        self.assertEqual(settings.calibtic_backend, "xml")

if __name__ == '__main__':
    unittest.main()
