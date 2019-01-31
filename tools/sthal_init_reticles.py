#!/usr/bin/env python
"""
This tool initializes all reticle on a wafer, frist a high-speed init is tries,
if this fails it falls back to JTAG
"""

import os
import argparse
import distutils.spawn
import re
import subprocess

from tabulate import tabulate

import Coordinate as C

REQUIRED_EXECUTABLES_ON_PATH = ["tests2", "reticle_init.py"]


def start_jobs(wafer, fpga, skip_fg=False, zero_fg=False, freq=125e6, reservation=None, defects_path=""):
    ip = fpga // 12 * 32 + fpga % 12 + 1

    os.environ["SINGULARITYENV_LD_LIBRARY_PATH"] = os.environ["LD_LIBRARY_PATH"]
    os.environ["SINGULARITYENV_PREPEND_PATH"] = os.environ["PATH"]

    container_cmd = "singularity exec --app {} {}".format(os.environ["CONTAINER_APP_NMPM_SOFTWARE"],
                                                          os.environ["CONTAINER_IMAGE_NMPM_SOFTWARE"])

    hs_cmd = "reticle_init.py --wafer {} --fpga {}".format(wafer, fpga)
    if skip_fg:
        hs_cmd += " --config_fpga_only"
    if zero_fg:
        hs_cmd += " --zero-floating-gate"
    if defects_path:
        hs_cmd += " --defects_path {}".format(defects_path)

    hs_cmd += " --freq {}".format(freq)
    jt_cmds = ["tests2 -bje 8 {} -jp 1700 -k7 -ip 192.168.{}.{} -m tm_hicann_sram_reset".format(h, wafer, ip)
               for h in range(C.HICANNOnDNC.enum_type.end)]

    logfile = 'high_speed_reticle_init_{:0>2}_{:0>2}.out'.format(wafer, fpga)
    hs_out = subprocess.check_output([
        'sbatch', '-p', 'calib',
        '--wmod', str(wafer),
        '--fpga-without-aout', str(fpga),
        '-o', logfile,
        '-t', '5' if not skip_fg else '1',
        '--mem', '500m', # 30GiB per node / 48 processes = 625MiB
        '-J', 'w{}f{}_hs_init'.format(wafer, fpga),
        '--wrap', "{} {}".format(container_cmd, hs_cmd)] + (['--reservation', reservation] if reservation else []))
    hs_jobid = re.search(r'\d+', hs_out).group(0)

    jt_jobids = []
    for h, jt_cmd in enumerate(jt_cmds):
        logfile = 'jtag_reticle_init_{:0>2}_{:0>2}_{:0>2}.out'.format(wafer, fpga, h)
        jt_out = subprocess.check_output([
            'sbatch', '-p', 'calib',
            '--wmod', str(wafer),
            '--fpga-without-aout', str(fpga),
            '-o', logfile,
            '--dependency', "afternotok:{}".format(hs_jobid), '--kill-on-invalid-dep=yes',
            '--mem', '500m', # see above
            '-t', '2',
            '-J', 'w{}f{}_jtag_init'.format(wafer, fpga),
            '--wrap', "{} {}".format(container_cmd, jt_cmd)]  + (['--reservation', reservation] if reservation else []))
        jt_jobid = re.search(r'\d+', jt_out).group(0)
        jt_jobids.append(jt_jobid)

    return [hs_jobid], jt_jobids


def process_line(x):
    result = x.split('|')
    jobid = result[0]
    status = result[1]
    exitcode = result[2].split(':')[0]
    return jobid, (status, exitcode)


def print_results(fpgas, job_ids, wafer):
    all_job_ids = sum([j for j in sum(job_ids,tuple())], [])
    out = subprocess.check_output([
        'sacct', '-X', '-n', '-j', ','.join(all_job_ids), '-P',
        '-o', 'JobId,State,ExitCode'])
    results = dict(process_line(line) for line in out.splitlines())

    lines = []
    for n, (hs_jobids, jt_jobids) in enumerate(job_ids):
        fpga = fpgas[n]
        hs_results = []
        for hs_jobid in hs_jobids:
            status, exitcode = results[hs_jobid]
            if status in ("COMPLETED", "FAILED"):
                hs_result = 'ok' if exitcode == "0" else exitcode
            else:
                hs_result = status
            hs_results.append(hs_result)

        jt_results = []
        for jt_jobid in jt_jobids:
            status, exitcode = results[jt_jobid]
            if status in ("COMPLETED", "FAILED"):
                jt_result = 'ok' if exitcode == "0" else exitcode
            elif status == "CANCELLED":
                jt_result = '-'
            else:
                jt_result = status
            jt_results.append(jt_result)

        fpga_global = C.FPGAGlobal(C.FPGAOnWafer(C.Enum(fpga)),C.Wafer(wafer))
        reticle = C.DNCOnFPGA(0).toDNCOnWafer(fpga_global).id().value()

        lines.append(("{:02d}".format(reticle),
                      "{:02d}".format(fpga),
                      " ".join(hs_results), " ".join(jt_results),
                      "-" if all([jt_result in ["-", "ok"]
                                  for jt_result in jt_results])
                      else "power down"))

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
    parser.add_argument("--defects_path",
                        default="/wang/data/calibration/brainscales/default",
                        help="path to defect data (needs pyredman)")
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
        jobs.append(start_jobs(args.wafer, fpga, args.skip_fg, args.zero_fg, args.freq, args.reservation, args.defects_path))

    all_jobs = sum([j for j in sum(jobs,tuple())], [])

    # Blocker
    print "Waiting for jobs  to complete... "
    subprocess.call([
         'srun', '-p', 'short', '-J', 'w{}_init'.format(args.wafer),
        '--dependency', 'afterany:' + ':'.join(all_jobs),
         '--', 'true'])

    print_results(args.fpga, jobs, args.wafer)

if __name__ == '__main__':
    main()
