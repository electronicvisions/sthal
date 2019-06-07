#!/usr/bin/env python
"""
Tool that configures all HICANNs with default settings.

You have to pass excatly one of the options --hicann, --fpga or --dnc
"""

import numpy as np
import argparse
import sys

import pylogging
pylogging.default_config(date_format='absolute')
pylogging.set_loglevel(pylogging.get("sthal"), pylogging.LogLevel.INFO)
pylogging.set_loglevel(pylogging.get("Default"), pylogging.LogLevel.INFO)
pylogging.set_loglevel(pylogging.get("halbe"), pylogging.LogLevel.INFO)

import pysthal
import pyhalbe
import Coordinate as C

from pysthal.command_line_util import add_default_coordinate_options
from pysthal.command_line_util import add_fpga_coordinate_options
from pysthal.command_line_util import add_logger_options
from pysthal.command_line_util import init_logger

class ConfigFPGAOnlyHICANNConfigurator(pysthal.HICANNConfigurator):
    """
    overwrite config to do nothing, config_fpga is untouched and will
    initializes all HICANNs on the reticle
    """

    def config(self, fpga_handle, hicann_handle, hicann_data):
        pass


def set_floating_gate_to_zero(hicann):
    """Set all floating gate values to zero"""
    fgc = hicann.floating_gates
    for block in C.iter_all(C.FGBlockOnHICANN):
        blk = fgc[block]
        for cell in C.iter_all(C.FGCellOnFGBlock):
            blk.setRaw(cell, 0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=__doc__, conflict_handler='resolve')
    parser.add_argument('--wafer', type=int, required=True,
                        help="Wafer enum on which reticles are to be initialized")
    parser.add_argument('--fpga', type=int, required=True, nargs='+',
                        help="FPGAOnWafer enum(s) for which reticles are to be initialized")
    parser.add_argument(
        '--hwdb', type=str, default=None,
        help="full path to hardware database")
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        '-z', '--zero-floating-gate', action='store_true', dest='zero_fg',
        help="set floating gate values to zero")
    group.add_argument(
        '-f', "--config_fpga_only", action="store_true")
    parser.add_argument("--freq", type=float, default=125e6)

    parser.add_argument("--defects_path",
                        default="/wang/data/calibration/brainscales/default",
                        help="path to defect data (needs pyredman)")

    add_logger_options(parser)

    args = parser.parse_args()

    WAFER = C.Wafer(args.wafer)

    settings = pysthal.Settings.get()
    settings.defects_host = args.defects_path
    if args.hwdb:
        settings.yaml_hardware_database_path = args.hwdb
        print "using non-default hardware database {}".format(args.hwdb)

    w = pysthal.Wafer(WAFER)
    # initialization of reticles is independent so we don't need global operation mode if we
    # have multiple reticles to init
    w.force_listen_local(True)
    w.commonFPGASettings().setPLL(args.freq)

    for fpga in [C.FPGAOnWafer(C.Enum(f)) for f in args.fpga]:
        DNC = fpga.toDNCOnWafer()
        for hicann_on_dnc in C.iter_all(C.HICANNOnDNC):
            if w.has(hicann_on_dnc.toHICANNOnWafer(DNC)):
                h = w[hicann_on_dnc.toHICANNOnWafer(DNC)]
                if args.zero_fg:
                    set_floating_gate_to_zero(h)

    w.connect(pysthal.MagicHardwareDatabase())

    if not args.config_fpga_only:
        w.configure() # use default configuration to allow for multithreading
    else:
        configurator = ConfigFPGAOnlyHICANNConfigurator()
        w.configure(configurator)


