#!/usr/bin/env python

import os
import argparse
import subprocess

import Coordinate as C
from pyredman.load import load

parser = argparse.ArgumentParser()
parser.add_argument("--wafer", type=int, help="Wafer enum", required=True)
parser.add_argument("--hicann", type=int, help="HICANNOnWafer enum", nargs="+", required=True)
parser.add_argument("--backend_path_pattern", type=str, help="redman backend path pattern", default="w{}h{}")
args = parser.parse_args()

for h in args.hicann:
    backend_path = args.backend_path_pattern.format(args.wafer, h)
    os.mkdir(backend_path)
    with open(os.path.join(backend_path, "out.txt"),'w') as fout, open(os.path.join(backend_path, "err.txt"),'w') as ferr:
        subprocess.call(["./sthal_single_chip_comm_test.py", "--wafer", str(args.wafer), "--hicann", str(h), "--backend_path", backend_path],
                        stdout=fout, stderr=ferr)
