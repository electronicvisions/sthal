#!/usr/bin/env python
"""
Tool that inits a single HICANN
"""

import argparse

import pylogging
pylogging.default_config(date_format='absolute')
pylogging.set_loglevel(pylogging.get("sthal"), pylogging.LogLevel.INFO)
pylogging.set_loglevel(pylogging.get("Default"), pylogging.LogLevel.INFO)
pylogging.set_loglevel(pylogging.get("halbe"), pylogging.LogLevel.INFO)

import Coordinate as C
import pysthal

def set_floating_gate_to_zero(hicann):
    """Set all floating gate values to zero"""
    fgc = hicann.floating_gates
    for block in C.iter_all(C.FGBlockOnHICANN):
        blk = fgc[block]
        for cell in C.iter_all(C.FGCellOnFGBlock):
            blk.setRaw(cell, 0)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wafer', type=int, required=True,
                        help="Wafer enum on which HICANN is to be initialized")
    parser.add_argument('--hicann', type=int, required=True,
                        help="HICANNOnWafer enum be initialized")
    parser.add_argument('--jtag', action="store_true",
                        help="Use jtag (highspeed is default)")
    parser.add_argument(
        '-z', '--zero-floating-gate', action='store_true', dest='zero_fg',
        help="set floating gate values to zero")

    args = parser.parse_args()
    wafer_c = C.Wafer(args.wafer)
    hicann_c = C.HICANNOnWafer(C.Enum(args.hicann))
    fpga_c = hicann_c.toFPGAOnWafer()
    dnc_c = fpga_c.toDNCOnWafer()

    wafer = pysthal.Wafer(wafer_c)
    wafer.drop_defects()

    fpga = wafer[fpga_c]
    hicann = wafer[hicann_c]

    if args.zero_fg:
        set_floating_gate_to_zero(hicann)

    # blacklist all but the HICANN under test
    for h in C.iter_all(C.HICANNOnDNC):
        if h.toHICANNOnWafer(fpga_c) != hicann_c:
            fpga.setBlacklisted(h, True)

    if args.jtag:
        for h_hs in C.iter_all(C.HighspeedLinkOnDNC):
            if h_hs.toHICANNOnDNC().toHICANNOnWafer(dnc_c) == hicann_c:
                fpga.setHighspeed(h_hs.toHICANNOnDNC(), False)

    wafer.connect(pysthal.MagicHardwareDatabase())
    if not args.zero_fg:
        wafer.configure(pysthal.JustResetHICANNConfigurator())
    else:
        wafer.configure()
