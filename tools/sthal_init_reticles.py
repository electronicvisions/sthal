#!/usr/bin/env python
"""
This tool initializes all reticle on a wafer, frist a high-speed init is tries,
if this fails it falls back to JTAG
"""

import argparse
import distutils.spawn
import re
import subprocess

from tabulate import tabulate

import Coordinate as C

REQUIRED_EXECUTABLES_ON_PATH = ["tests2", "reticle_init.py"]


def start_jobs(wafer, fpga, skip_fg=False, zero_fg=False, freq=125e6, reservation=None):
    ip = fpga // 12 * 32 + fpga % 12 + 1

    hs_cmd = "reticle_init.py --wafer {} --fpga {}".format(wafer, fpga)
    if skip_fg:
        hs_cmd += " --config_fpga_only"
    if zero_fg:
        hs_cmd += " --zero-floating-gate"

    hs_cmd += " --freq {}".format(freq)
    jt_cmd = "hostname; tests2 -bje2fa 8 0 -jp 1700 -k7 -ip 192.168.{}.{} -m tm_hicann_sram_reset".format(wafer, ip)

    logfile = 'high_speed_reticle_init_{:0>2}_{:0>2}.out'.format(wafer, fpga)
    hs_out = subprocess.check_output([
        'sbatch', '-p', 'experiment',
        '--wmod', str(wafer),
        '--fpga-without-aout', str(fpga),
        '-o', logfile,
        '-t', '5' if not skip_fg else '1',
        '--mem', '500m', # 30GiB per node / 48 processes = 625MiB
        '-J', 'w{}f{}_hs_init'.format(wafer, fpga),
        '--wrap', hs_cmd] + (['--reservation', reservation] if reservation else []))
    hs_jobid = re.search(r'\d+', hs_out).group(0)

    logfile = 'jtag_reticle_init_{:0>2}_{:0>2}.out'.format(wafer, fpga)
    jt_out = subprocess.check_output([
        'sbatch', '-p', 'experiment',
        '--wmod', str(wafer),
        '--fpga-without-aout', str(fpga),
        '-o', logfile,
        '--dependency', "afternotok:{}".format(hs_jobid), '--kill-on-invalid-dep=yes',
        '--mem', '500m', # see above
        '-t', '2',
        '-J', 'w{}f{}_jtag_init'.format(wafer, fpga),
        '--wrap', jt_cmd]  + (['--reservation', reservation] if reservation else []))
    jt_jobid = re.search(r'\d+', jt_out).group(0)

    return hs_jobid, jt_jobid


def process_line(x):
    result = x.split('|')
    jobid = result[0]
    status = result[1]
    exitcode = result[2].split(':')[0]
    return jobid, (status, exitcode)


def print_results(fpgas, job_ids, wafer):
    all_job_ids = sum(job_ids, tuple())
    out = subprocess.check_output([
        'sacct', '-X', '-n', '-j', ','.join(all_job_ids), '-P',
        '-o', 'JobId,State,ExitCode'])
    results = dict(process_line(line) for line in out.splitlines())

    lines = []
    for n, (hs_jobid, jt_jobid) in enumerate(job_ids):
        fpga = fpgas[n]
        status, exitcode = results[hs_jobid]
        if status in ("COMPLETED", "FAILED"):
            hs_result = 'ok' if exitcode == "0" else exitcode
        else:
            hs_result = status

        status, exitcode = results[jt_jobid]
        if status in ("COMPLETED", "FAILED"):
            jt_result = 'ok' if exitcode == "0" else exitcode
        elif status == "CANCELLED":
            jt_result = '-'
        else:
            jt_result = status

        fpga_global = C.FPGAGlobal(C.FPGAOnWafer(C.Enum(fpga)),C.Wafer(wafer))
        reticle = C.DNCOnFPGA(0).toDNCOnWafer(fpga_global).id().value()

        lines.append(("{:02d}".format(reticle),
                      "{:02d}".format(fpga),
                      hs_result, jt_result,
                      "-" if jt_result in ["-", "ok"] else "power down"))

    print tabulate(lines, headers=["RETICLE", "FPGA", "HS", "JTAG", "ACTION"])


def main():
    """
    initializes HICANNs via sthal and high speed links and via jtag as fallback
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('wafer', type=int, help='wafer to init')
    parser.add_argument('--fpga', type=int, default=range(48), nargs="*")
    group = parser.add_mutually_exclusive_group()
    group.add_argument('-s', '--skip_fg', action="store_true",
                       help='skip floating gate programming')
    group.add_argument('-z', '--zero_fg', action='store_true',
                       help="set floating gate values to zero")
    parser.add_argument("--reservation", help="name of slurm reservation",
                        default=None)
    parser.add_argument("--freq", type=float, default=125e6)
    args = parser.parse_args()

    unmet_dependencies = [
        name for name in REQUIRED_EXECUTABLES_ON_PATH
        if not distutils.spawn.find_executable(name)
    ]

    if unmet_dependencies:
        parser.error(
            "required executable(s) not found: {}".format(
                ", ".join(unmet_dependencies)))

    print "Running on Wafer {}...".format(args.wafer)

    jobs = []
    for fpga in args.fpga:
        jobs.append(start_jobs(args.wafer, fpga, args.skip_fg, args.zero_fg, args.freq, args.reservation))

    # Blocker
    print "Waiting for jobs  to complete... "
    subprocess.call([
         'srun', '-p', 'short', '-J', 'w{}_init'.format(args.wafer),
        '--dependency', 'afterany:' + ':'.join(j[1] for j in jobs),
         '--', 'true'])

    print_results(args.fpga, jobs, args.wafer)

if __name__ == '__main__':
    main()
