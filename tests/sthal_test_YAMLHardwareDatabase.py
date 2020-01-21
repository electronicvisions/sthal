#!/usr/bin/env python

import unittest
import tempfile
import os
import re

# TODO remove
import warnings
warnings.simplefilter("ignore")

import pysthal
from HWTest import PyhalbeTest

from pyhalco_hicann_v2 import iter_all
from pyhalco_hicann_v2 import AnalogOnHICANN
from pyhalbe.ADC import USBSerial as ADC
from pyhalco_hicann_v2 import ChannelOnADC
from pyhalco_hicann_v2 import TriggerOnADC
from pyhalco_common import Enum
from pyhalco_hicann_v2 import FPGAGlobal
from pyhalco_hicann_v2 import FPGAOnWafer
from pyhalco_hicann_v2 import Wafer
from pyhalco_hicann_v2 import IPv4
from pyhalco_hicann_v2 import SetupType
from pyhalco_hicann_v2 import HICANNGlobal
from pyhalco_hicann_v2 import HICANNOnWafer

import pylogging

class TestYAMLHardwareDatabase(PyhalbeTest):
    def setUp(self):
        pylogging.reset()
        pylogging.default_config(pylogging.LogLevel.WARN)

    def test_empyt_db(self):
        """
        Test setting and getting speedup.
        """
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            db.store(f.name)
            db.load(f.name)
            self.assertEqual(len(f.read()), 0)

    def test_manual_YAML(self):
        """
        Test setting and getting speedup.
        """
        yaml = """
---
wafer: 0
setuptype: facetswafer
fpgas:
  - {fpga: 0, ip: 192.168.1.1}
  - fpga: 1
    ip: "192.168.1.3"
  - fpga: 2
    ip: "192.168.1.5"
  - fpga: 3
    ip: "192.168.1.7"
  - fpga: 4
    ip: "192.168.1.19"
  - fpga: 8
    ip: "192.168.1.17"
  - fpga: 9
    ip: "192.168.1.9"
  - fpga: 10
    ip: "192.168.1.21"
  - fpga: 11
    ip: "192.168.1.23"
"""
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            db.load(f.name)

    def test_manual_YAML2(self):
        """
        Test minimal wafer entry and setup types case-insensitivty
        """
        yaml = """
---
wafer: 1
setuptype: VSetup
---
wafer: 0
setuptype: facetswafer
---
wafer: 6
setuptype: CuBeSeTuP
---
wafer: 20
setuptype: BSSWafer
macu: 0.0.0.0
macuversion: 1
"""
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            db.load(f.name)
        self.assertEqual(SetupType.FACETSWafer, db.get_setup_type(Wafer(0)))
        self.assertEqual(SetupType.VSetup, db.get_setup_type(Wafer(1)))
        self.assertEqual(SetupType.CubeSetup, db.get_setup_type(Wafer(6)))
        self.assertEqual(SetupType.BSSWafer, db.get_setup_type(Wafer(20)))

    def test_manual_YAML3(self):
        """
        Test hicann shortcut notation
        """
        yaml = """
---
wafer: 1
setuptype: facetswafer
fpgas:
  - fpga: 8
    ip: "192.168.2.17"
hicanns:
  version: 2
"""
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            db.load(f.name)
        self.assertEqual(SetupType.FACETSWafer, db.get_setup_type(Wafer(1)))
        for hicann in iter_all(HICANNOnWafer):
            hicann_global = HICANNGlobal(hicann, Wafer(1))
            if hicann_global.toFPGAOnWafer() == FPGAOnWafer(8):
                self.assertTrue(db.has_hicann(hicann_global),
                                "HICANN {} should be available".format(hicann))
            else:
                self.assertFalse(db.has_hicann(hicann_global),
                                 "HICANN {} should not be available".format(hicann))

    def test_manual_YAML4(self):
        """
        Test minimal wafer entry and macu
        """
        yaml = """
---
wafer: 0
setuptype: bsswafer
macu: 192.168.5.3
macuversion: 1
---
wafer: 1
setuptype: bsswafer
---
wafer: 2
setuptype: cubesetup
"""
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            self.assertRaises(RuntimeError, db.load, f.name)
        yaml = """
---
wafer: 0
setuptype: bsswafer
macu: 192.168.5.3
macuversion: 1
---
wafer: 2
setuptype: cubesetup
"""
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            db.load(f.name)
        self.assertEqual(IPv4.from_string("192.168.5.3"), db.get_macu(Wafer(0)))
        self.assertEqual(IPv4(), db.get_macu(Wafer(2)))

    def test_MagicHardwareDatabase(self):
        """
        Test using the magic hardware database with custom backend file
        """
        yaml = """
---
wafer: 0
setuptype: facetswafer
fpgas:
  - fpga: 0
    ip: 10.11.12.13
  - fpga: 1
    ip: "127.127.127.127" # BOGUS ips, otherwise the orignal database is loaded and we don't see it
"""

        settings = pysthal.Settings.get()
        original_yaml_path = settings.yaml_hardware_database_path
        def cleanup(settings, path):
            settings.yaml_hardware_database_path = path

        self.addCleanup(cleanup, settings, original_yaml_path)


        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            settings.yaml_hardware_database_path = f.name

            db = pysthal.MagicHardwareDatabase()
            self.assertEqual(
                db.get_fpga_ip(FPGAGlobal(FPGAOnWafer(0), Wafer(0))),
                IPv4.from_string("10.11.12.13"))
            self.assertEqual(
                db.get_fpga_ip(FPGAGlobal(FPGAOnWafer(1), Wafer(0))),
                IPv4.from_string("127.127.127.127"))

    def test_interface(self):
        """
        Test adding and removing entries
        """
        db = pysthal.YAMLHardwareDatabase()
        db.add_wafer(Wafer(4), SetupType.CubeSetup)
        db.add_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(4)),
                               IPv4.from_string("192.168.4.1"))
        db.add_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(4)),
                               IPv4.from_string("192.168.4.4"))
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4)), 2, "X")
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(89)), Wafer(4)), 4, "v4-42")
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(116)), Wafer(4)), 2, "X")
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(4)), 4)
        db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(4)),
                              AnalogOnHICANN(0), ADC("B201287"), ChannelOnADC(1), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(4)),
                              AnalogOnHICANN(1), ADC("B201287"), ChannelOnADC(0), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(3),  Wafer(4)),
                              AnalogOnHICANN(0), ADC("B201254"), ChannelOnADC(1), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(3),  Wafer(4)),
                              AnalogOnHICANN(1), ADC("B201254"), ChannelOnADC(0), TriggerOnADC(0))

        db.add_wafer(Wafer(5), SetupType.CubeSetup)
        db.add_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(5)),
                               IPv4.from_string("192.168.5.1"))
        db.add_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(5)),
                               IPv4.from_string("192.168.5.4"))
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(5)), 2)
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(5)), 4)
        db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(5)),
                              AnalogOnHICANN(0), ADC("B201280"), ChannelOnADC(0), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(5)),
                              AnalogOnHICANN(1), ADC("B201280"), ChannelOnADC(1), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(3),  Wafer(5)),
                              AnalogOnHICANN(0), ADC("B201312"), ChannelOnADC(0), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(3),  Wafer(5)),
                              AnalogOnHICANN(1), ADC("B201312"), ChannelOnADC(1), TriggerOnADC(0))

        db.add_wafer(Wafer(6), SetupType.CubeSetup)
        db.add_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(6)),
                               IPv4.from_string("192.168.6.1"))
        db.add_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(6)),
                               IPv4.from_string("192.168.6.4"))
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(6)), 2, "X")
        db.add_hicann(
            HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(6)), 4)
        db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(6)),
                              AnalogOnHICANN(0), ADC("B201215"), ChannelOnADC(0), TriggerOnADC(0))
        db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(6)),
                              AnalogOnHICANN(1), ADC("B201215"), ChannelOnADC(1), TriggerOnADC(0))

        db.add_wafer(Wafer(7), SetupType.BSSWafer, IPv4.from_string("192.168.6.5"))

        def check(self, db):
            self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(4))))
            self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(4))))
            self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(5))))
            self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(5))))
            self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(6))))
            self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(6))))

            self.assertTrue(
                db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4))))
            self.assertTrue(
                db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(4))))
            self.assertTrue(
                db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(5))))
            self.assertTrue(
                db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(5))))
            self.assertTrue(
                db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(6))))
            self.assertTrue(
                db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(6))))

            hicann = HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(4))
            self.assertEqual(hicann.toFPGAGlobal(), FPGAGlobal(FPGAOnWafer(0),  Wafer(4)))
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(0))
            self.assertEqual(cfg.coord, ADC("B201287"))
            self.assertEqual(cfg.channel, ChannelOnADC(1))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(1))
            self.assertEqual(cfg.coord, ADC("B201287"))
            self.assertEqual(cfg.channel, ChannelOnADC(0))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)

            hicann = HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4))
            self.assertEqual(hicann.toFPGAGlobal(), FPGAGlobal(FPGAOnWafer(3),  Wafer(4)))
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(0))
            self.assertEqual(cfg.coord, ADC("B201254"))
            self.assertEqual(cfg.channel, ChannelOnADC(1))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(1))
            self.assertEqual(cfg.coord, ADC("B201254"))
            self.assertEqual(cfg.channel, ChannelOnADC(0))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)

            hicann = HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(5))
            self.assertEqual(hicann.toFPGAGlobal(), FPGAGlobal(FPGAOnWafer(0),  Wafer(5)))
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(0))
            self.assertEqual(cfg.coord, ADC("B201280"))
            self.assertEqual(cfg.channel, ChannelOnADC(0))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(1))
            self.assertEqual(cfg.coord, ADC("B201280"))
            self.assertEqual(cfg.channel, ChannelOnADC(1))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)

            hicann = HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(5))
            self.assertEqual(hicann.toFPGAGlobal(), FPGAGlobal(FPGAOnWafer(3),  Wafer(5)))
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(0))
            self.assertEqual(cfg.coord, ADC("B201312"))
            self.assertEqual(cfg.channel, ChannelOnADC(0))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(1))
            self.assertEqual(cfg.coord, ADC("B201312"))
            self.assertEqual(cfg.channel, ChannelOnADC(1))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)

            hicann = HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(6))
            self.assertEqual(hicann.toFPGAGlobal(), FPGAGlobal(FPGAOnWafer(0),  Wafer(6)))
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(0))
            self.assertEqual(cfg.coord, ADC("B201215"))
            self.assertEqual(cfg.channel, ChannelOnADC(0))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
            cfg = db.get_adc_of_hicann(hicann, AnalogOnHICANN(1))
            self.assertEqual(cfg.coord, ADC("B201215"))
            self.assertEqual(cfg.channel, ChannelOnADC(1))
            self.assertEqual(cfg.trigger, TriggerOnADC(0))
            self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)

            self.assertEqual(db.get_macu(Wafer(6)), IPv4())
            self.assertEqual(db.get_macu(Wafer(7)), IPv4.from_string("192.168.6.5"))

        check(self, db)
        with tempfile.NamedTemporaryFile() as f:
            db.store(f.name)
            f.flush()
            test_db = pysthal.YAMLHardwareDatabase(f.name)
            check(self, test_db)

        # Check that HICANN is removed, when fpga is removed
        db.remove_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(4)))
        self.assertFalse(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(4))))
        self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(4))))
        self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(5))))
        self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(5))))
        self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(6))))
        self.assertTrue(db.has_fpga(FPGAGlobal(FPGAOnWafer(3),  Wafer(6))))

        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4))))
        self.assertFalse(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(4))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(5))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(5))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(6))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(6))))

        #Check HICANN removal
        db.remove_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4)))
        self.assertFalse(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4))))
        self.assertFalse(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(4))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(5))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(5))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(6))))
        self.assertTrue(
            db.has_hicann(HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(6))))

    def test_invalid_adds(self):
        """Check that errors a risen on invalid add calls"""
        db = pysthal.YAMLHardwareDatabase()

        with self.assertRaises(IndexError) as err:
            db.add_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(4)),
                                   IPv4.from_string("192.168.4.1"))
        self.assertEqual("map::at", str(err.exception))

        with self.assertRaises(IndexError) as err:
            db.add_hicann(
                HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4)), 2, "X")
        self.assertEqual("map::at", str(err.exception))

        with self.assertRaises(IndexError) as err:
            db.add_adc(FPGAGlobal(FPGAOnWafer(0),  Wafer(4)),
                                  AnalogOnHICANN(0), ADC("B201287"),
                                  ChannelOnADC(1), TriggerOnADC(0))
        self.assertEqual("map::at", str(err.exception))

        db.add_wafer(Wafer(4), SetupType.CubeSetup)
        self.assertFalse(db.has_fpga(FPGAGlobal(FPGAOnWafer(0),  Wafer(4))))
        with self.assertRaises(IndexError) as err:
            db.add_hicann(
                HICANNGlobal(HICANNOnWafer(Enum(88)), Wafer(4)), 2, "X")
        self.assertIn("map::at", str(err.exception))

    def test_hicann_merge(self):
        db = pysthal.YAMLHardwareDatabase()
        wafer = Wafer(20)
        db.add_wafer(wafer, SetupType.BSSWafer)
        for fpga in iter_all(FPGAOnWafer):
            db.add_fpga(
                FPGAGlobal(fpga, wafer),
                IPv4.from_string("192.168.20.{}".format(int(fpga) + 1)))
        for hicann in iter_all(HICANNOnWafer):
            db.add_hicann(HICANNGlobal(hicann, wafer), 4)
        yaml = str(db)
        # All HICANN entries should be merged into a single entry
        self.assertEqual(len(re.findall('hicann:', yaml)), 0)
        self.assertEqual(len(re.findall('hicanns:', yaml)), 1)
        self.assertEqual(len(re.findall('version: 4', yaml)), 1)
        self.assertEqual(len(re.findall('fpga:', yaml)), 48)

    def test_adc_factory(self):
        """
        Test that the ADC config is complete an has a factory set
        """
        yaml = """
---
wafer: 20
setuptype: BSSWafer
macu: 0.0.0.0
macuversion: 1
fpgas:
  - fpga: 0
    ip: 192.168.1.1
adcs:
  - fpga: 0
    dnc_on_fpga: 0
    analog: 0
    adc: "03"
    channel: 6
    trigger: 1
  - fpga: 0
    dnc_on_fpga: 0
    analog: 1
    adc: "04"
    remote_ip: "123.123.123.123"
    remote_port: 321
    channel: 6
    trigger: 1
"""
        db = pysthal.YAMLHardwareDatabase()
        with tempfile.NamedTemporaryFile() as f:
            f.write(yaml)
            f.flush()
            db.load(f.name)

        hicann = HICANNGlobal(HICANNOnWafer(Enum(144)), Wafer(20))
        analog = AnalogOnHICANN(0)
        cfg = db.get_adc_of_hicann(hicann, analog)
        self.assertEqual(cfg.channel, ChannelOnADC(6))
        self.assertEqual(cfg.trigger, TriggerOnADC(1))
        self.assertEqual(cfg.coord, ADC("03"))
        self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
        with self.assertRaises(TypeError):
            # If cfg.factory would be a null pointer cfg.factory would be None
            # and no error would be raised
            f = cfg.factory
        analog = AnalogOnHICANN(1)
        cfg = db.get_adc_of_hicann(hicann, analog)
        self.assertEqual(cfg.channel, ChannelOnADC(6))
        self.assertEqual(cfg.trigger, TriggerOnADC(1))
        self.assertEqual(cfg.coord, ADC("04"))
        self.assertEqual(cfg.loadCalibration, pysthal.ADCConfig.CalibrationMode.LOAD_CALIBRATION)
        with self.assertRaises(TypeError):
            # If cfg.factory would be a null pointer cfg.factory would be None
            # and no error would be raised
            f = cfg.factory

if __name__ == '__main__':
    unittest.main()
