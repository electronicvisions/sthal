import unittest
import sys
import functools
import warnings

import pysthal
import pyhalbe
import pylogging

import pyhalco_hicann_v2 as Coord

from pyhalco_common import Enum
from pyhalco_hicann_v2 import SynapseDriverOnHICANN
from pyhalco_hicann_v2 import SynapseRowOnHICANN
from pyhalco_hicann_v2 import DNCMergerOnHICANN
from pyhalco_hicann_v2 import GbitLinkOnHICANN

from pysthal.command_line_util import add_default_coordinate_options
from pysthal.command_line_util import add_logger_options
from pysthal.command_line_util import init_logger


def hardware(test):
    """skips test if no hardware is given, use in test cases inherited from PysthalTest"""
    @functools.wraps(test)
    def wrapper(self, *args, **kwargs):
        attrs = (self.HICANN, self.WAFER)
        if self.RUN == False or None in attrs:
            raise unittest.SkipTest("No HICANN selected, skipping hardware test")
        else:
            test(self, *args, **kwargs)
    return wrapper

def expensive(test):
    """Skip tests that take long (>~1min) and/or need a lot of memory (>~4GB)"""
    @functools.wraps(test)
    def wrapper(self, *args, **kwargs):
        if self.SKIP_EXPENSIVE:
            raise unittest.SkipTest("Skip expensive test")
        else:
            test(self, *args, **kwargs)
    return wrapper

recorder_list = []
_analogRecorder = pysthal.HICANN.analogRecorder
def analogRecorder(*args, **kwargs):
    tmp = _analogRecorder(*args, **kwargs)
    recorder_list.append(tmp)
    return tmp
pysthal.HICANN.analogRecorder = analogRecorder

def cleanupRecorder():
    for recorder in recorder_list:
        recorder.freeHandle()
    recorder_list[:] = []


class PysthalTest(unittest.TestCase):
    HICANN = None
    WAFER = None
    RUN = True
    PREOUT_DRIVERS = None
    PREOUT_OUTPUT = None
    PREOUT_LINK = None
    COORDINATE_BLACKLIST = ()
    FPGA_HICANN_DELAY = 0


    def setUp(self):
        self.addCleanup(cleanupRecorder)

    @classmethod
    def main(self, *was_args, **was_kwargs):
        import argparse
        init_logger('INFO')

        parser = argparse.ArgumentParser(
                description='SthalHWTest: %s' % self.__name__)
        add_default_coordinate_options(parser)
        parser.add_argument('--v4', action='store_true', default=False,
                            help="Run tests for HICANNv4")
        parser.add_argument('--hwdb', type=str, default=None, help="full path to hardware database")
        parser.add_argument('--xml-output-dir', type=str, default=None,
                            help='create xml reports')
        parser.add_argument('--skip-expensive', action='store_true',
                            help=expensive.__doc__)
        add_logger_options(parser)

        args, argv = parser.parse_known_args()
        argv.insert(0, sys.argv[0])

        settings = pysthal.Settings.get()
        if args.hwdb:
            settings.yaml_hardware_database_path = args.hwdb
            print("using non-default hardware database {}".format(args.hwdb))

        self.HICANN = args.hicann
        self.WAFER = args.wafer
        self.SKIP_EXPENSIVE = args.skip_expensive
        if args.v4:
            self.PREOUT_DRIVERS = (SynapseDriverOnHICANN(Enum(109)),
                                   SynapseDriverOnHICANN(Enum(114)))
            self.PREOUT_OUTPUT = DNCMergerOnHICANN(6)
            self.PREOUT_LINK = GbitLinkOnHICANN(6)
            # Coordinates not available on HICANN v4
            self.COORDINATE_BLACKLIST = (
                SynapseDriverOnHICANN(Enum(110)),
                SynapseDriverOnHICANN(Enum(111)),
                SynapseDriverOnHICANN(Enum(112)),
                SynapseDriverOnHICANN(Enum(113)),
                SynapseRowOnHICANN(220),
                SynapseRowOnHICANN(221),
                SynapseRowOnHICANN(222),
                SynapseRowOnHICANN(223),
                SynapseRowOnHICANN(224),
                SynapseRowOnHICANN(225),
                SynapseRowOnHICANN(226),
                SynapseRowOnHICANN(227),
            )
            self.HICANN_VERSION = 4
        else:
            self.PREOUT_DRIVERS = (SynapseDriverOnHICANN(Enum(111)),
                                   SynapseDriverOnHICANN(Enum(112)))
            self.PREOUT_OUTPUT = DNCMergerOnHICANN(7)
            self.PREOUT_LINK = GbitLinkOnHICANN(7)
            self.COORDINATE_BLACKLIST = tuple()
            self.HICANN_VERSION = 2

        test_runner = None
        if args.xml_output_dir:
            from xmlrunner import XMLTestRunner
            test_runner = XMLTestRunner(output=args.xml_output_dir)

        unittest.main(argv=argv, testRunner=test_runner, *was_args, **was_kwargs)
