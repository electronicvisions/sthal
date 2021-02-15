#!/usr/bin/env python

import os
import argparse
import subprocess

from pyredman.load import load

parser = argparse.ArgumentParser()
parser.add_argument("--wafer", type=int, help="Wafer enum", required=True)
parser.add_argument("--hicann", type=int, help="HICANNOnWafer enum", nargs="+", required=True)
parser.add_argument("--backend_path_pattern", type=str, help="redman backend path pattern", default="w{}h{}")
parser.add_argument("--pll", type=int, default=125, help="Used Pll frequency (in MHz)")
parser.add_argument(
    '-zs', '--zero-synapses', action='store_true', dest='zero_syn',
    help="try to set synapse values to zero in highspeed test")
args = parser.parse_args()

for h in args.hicann:
    backend_path = args.backend_path_pattern.format(args.wafer, h)
    os.mkdir(backend_path)
    with open(os.path.join(backend_path, "out.txt"),'w') as fout, open(os.path.join(backend_path, "err.txt"),'w') as ferr:
        cmd = ["sthal_single_chip_comm_test.py", "--wafer", str(args.wafer), "--hicann", str(h), "--backend_path", backend_path, "--pll", str(args.pll)]
        if args.zero_syn:
            cmd.append("-zs")
        subprocess.call(cmd, stdout=fout, stderr=ferr)
