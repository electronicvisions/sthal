#!/usr/bin/env python

import os
import argparse
import subprocess
import glob

from pyhalco_common import Enum
import pyhalco_hicann_v2 as C
from pyredman.load import load

parser = argparse.ArgumentParser()
parser.add_argument("--wafer", type=int, help="Wafer enum", required=True)
parser.add_argument("--hicann", type=int, help="HICANNOnWafer enum", required=True)
parser.add_argument("--backend_path", type=str, help="redman backend path", required=True)
parser.add_argument("--skip_empty_backend_path_check", action="store_true")
parser.add_argument("--pll", type=int, default=125, help="Used Pll frequency (in MHz)")
parser.add_argument(
    '-zs', '--zero-synapses', action='store_true', dest='zero_syn',
    help="try to set synapse values to zero in highspeed test")
args = parser.parse_args()

if not args.skip_empty_backend_path_check and glob.glob(os.path.join(args.backend_path, "*.xml")):
    raise RuntimeError("{} is not empty".format(args.backend_path))

wafer_c = C.Wafer(args.wafer)
hicann_c = C.HICANNOnWafer(Enum(args.hicann))
fpga_c = hicann_c.toFPGAOnWafer()
fpga_global_c = C.FPGAGlobal(fpga_c, wafer_c)

redman_wafer_backend = load.WaferWithBackend(args.backend_path, wafer_c)
redman_fpga_backend = load.FpgaWithBackend(args.backend_path, fpga_global_c)
hs_exit = 0
jtag_exit = 0

# reset fpga before each HICANN test
fpga_reset_cmd = ["fpga_remote_init.py", "-r", "1", "-w", str(args.wafer), "-f", str(fpga_c.value()), "--alloc", "existing"]
subprocess.call(fpga_reset_cmd)
highspeed_test_cmd = ["sthal_single_chip_init.py", "--wafer", str(args.wafer), "--hicann", str(args.hicann), "--pll", str(args.pll)]
if args.zero_syn:
    highspeed_test_cmd.append("-zs")
hs_exit = subprocess.call(highspeed_test_cmd)
if hs_exit != 0:
    subprocess.call(fpga_reset_cmd)
    jtag_exit = subprocess.call(["sthal_single_chip_init.py", "--wafer", str(args.wafer), "--hicann", str(args.hicann), "--pll", str(args.pll), "--jtag"])

# highspeed failed, but jtag worked -> blacklist only highspeed
if hs_exit != 0 and jtag_exit == 0:
    redman_fpga_backend.hslinks().disable(hicann_c.toHighspeedLinkOnDNC())
    redman_fpga_backend.save()

# neither highspeed, nor jtag worked -> blacklist fully
if hs_exit != 0 and jtag_exit != 0:
    redman_wafer_backend.hicanns().disable(hicann_c)
    redman_wafer_backend.save()
