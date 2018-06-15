'''
Author: Christian Mauch
Module to log runtime of hable backend functions and corresbonding eth and
usb traffic. It gathers the traffic information from fundamental read/write
function in hicann-system and vmodule. It also cann log the raw taffic on wire
via wireshark.
To use this module just create a Perflogger object and pass the parse_args()
object from argpase as an argument to the Perflogger object. Set the wireshark
option true to log raw traffic.
The header should include the pystahl.command_line_util functions
    add_default_coordinate_options
    add_logger_options

Insert start() and stop() functions around the code block that should be logged

The logger is active if a --perflogpath argument was given. It specifies the
path where the log files are saved.

The logger creates a func_time_<logID>.log, pure_traffic_<logID>.log and
if the wireshark option is true a raw_traffic_<logID>.log
were function calls and ethernet/usb packets are logged.

!!! To be able to log the USB traffic with wireshark the usbmon module must be
activated on the corresponding host machine !!!:
sudo modprobe usbmon

The logID has the format:
w<WaferId>h<HicannNr>_<YY><MM><DD><hh><mm><ss>
start() optionally takes logId as an argument.

use sthal/tools/perflogger_analyze.py to visualize the data
'''

import pylogging
import threading
import subprocess
import time

from Coordinate import FPGAOnWafer
from Coordinate import Wafer
from Coordinate import gridLookupFPGAOnWafer
from Coordinate import DNCGlobal

class Perflogger():

    def __init__(self, args, wireshark = False):
        # variable to signal thread to stop
        self.exit_threads = False
        self.logpath = args.perflogpath
        self.wafer = args.wafer.value()
        self.hicann = args.hicann.id().value()
        self.fpga = FPGAOnWafer( gridLookupFPGAOnWafer( DNCGlobal( args.hicann.toDNCOnWafer(), args.wafer ) ) ).value()
        assert(type(wireshark)==bool)
        self.wireshark = wireshark

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.stop()

    #function to be threaded to start and kill wireshark log
    def subprocess_thread(self, command):
        ws_subprocess = subprocess.Popen(
                        command, shell=True, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
        while True:
            poll_ret = ws_subprocess.poll()
            if poll_ret is not None:
                break
            if self.exit_threads:
                ws_subprocess.kill()
                break
            time.sleep(.1)


    def start(self, logid = None):
        #if no logpath given don't log
        if not self.logpath:
            return

        # determine fpga ip
        if self.wafer < 3:
            # old wafer systems
            octet3 = self.wafer + 1
            octet4 = self.fpga * 2 + 1
        else:
            # new wafer systems
            octet3 = self.wafer
            octet4 = self.fpga % 12 * 32 + self.fpga + 1
        self.fpga_ip = "192.168." + str(octet3) + "." + str(octet4)

        #set file id
        if not logid:
            now = time.localtime(time.time())
            logid = "w{0}h{1}_{2:02d}{3:02d}{4:02d}{5:02d}{6:02d}{7:02d}".format(self.wafer, self.hicann, now[0], now[1], now[2],now[3], now[4], now[5])

        #init logger for backend functions
        #TODO: minimal log format (<function> <start_time> <end_time>)
        self.func_logger = pylogging.get("Timer")
        pylogging.write_to_file(
            self.logpath + "func_time_" + logid + ".log",
            append=False, logger=self.func_logger)
        pylogging.set_loglevel(self.func_logger, pylogging.LogLevel.TRACE)

        self.eth_logger = pylogging.get("hicann-system.CtrlModulePerf")
        self.usb_logger = pylogging.get("vmodule.usbcomPerf")
        traffic_appender = pylogging.FileAppender(
                   pylogging.ColorLayout(None), self.logpath + "pure_traffic_" + logid + ".log", False)
        self.eth_logger.setAdditivity(False)
        self.eth_logger.addAppender(traffic_appender)
        self.usb_logger.setAdditivity(False)
        self.usb_logger.addAppender(traffic_appender)
        pylogging.set_loglevel(self.usb_logger, pylogging.LogLevel.TRACE)
        pylogging.set_loglevel(self.eth_logger, pylogging.LogLevel.TRACE)

        # determine ethernet interface on machine based on wafer id
        if self.wafer > 4:
            interface = "skynet0"
        else:
            interface = "eth1"

        #start wireshark thread
        #TODO: break if tshark throws error
        self.exit_threads = False
        if self.wireshark:
            self.traffic_thread = threading.Thread(target=self.subprocess_thread,
                             args=("sudo tshark -t e -i " + interface + " -f"
                             " 'net " + self.fpga_ip + "' -i usbmon0 > "
                             " " + self.logpath + "raw_traffic_" + logid + ".log", ))
            self.traffic_thread.start()

    def stop(self):
        #if no logpath given don't log
        if not self.logpath:
            return
        self.exit_threads = True
        if self.wireshark:
            self.traffic_thread.join()
