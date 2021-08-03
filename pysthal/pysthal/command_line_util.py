import argparse
import pylogging
import collections
import os

from pyhalco_common import X, Y, Enum
from pyhalco_hicann_v2 import Wafer, HICANNOnDNC, DNCOnWafer, \
    DNCOnFPGA, FPGAGlobal, FPGAOnWafer, HICANNOnWafer, PowerCoordinate, \
    gridLookupDNCGlobal, AnalogOnHICANN

def parse_reticle(arg):
    """Helper to parse reticle coordinate, given as <recticle,hicann>"""
    try:
        ret, hicann = arg.split(",")
        power = PowerCoordinate(int(ret.strip()))
        hicann = HICANNOnDNC(Enum(int(hicann.strip())))
        return hicann.toHICANNOnWafer(power.toDNCOnWafer())
    except RuntimeError as err:
        raise argparse.ArgumentTypeError(str(err))
    except Exception:
        raise argparse.ArgumentTypeError(
            "Please provide --reticle <reticle,hicann>")

def parse_wafer(arg):
    """Helper to parse wafer coordinate"""
    return Wafer(int(arg))

def parse_hicann(arg):
    """Helper to parse hicann coordinate, given as <x,y> or as <enum>"""
    tmp = arg.split(",")
    if len(tmp) == 1:
        return HICANNOnWafer(Enum(int(tmp[0])))
    elif len(tmp) == 2:
        xcoord, ycoord = tmp
        return HICANNOnWafer(X(int(xcoord)), Y(int(ycoord)))
    else:
        raise argparse.ArgumentTypeError(
            "Please provide --hicann <x,y> or --hicann <enum>")

def parse_dnc(arg):
    """Helper to parse dnc coordinate, given as <x,y> or as <enum>"""
    tmp = arg.split(",")
    if len(tmp) == 1:
        return DNCOnWafer(Enum(int(tmp[0])))
    elif len(tmp) == 2:
        xcoord, ycoord = tmp
        return DNCOnWafer(X(int(xcoord)), Y(int(ycoord)))
    else:
        raise argparse.ArgumentTypeError(
            "Please provide --dnc <x,y> or --dnc <enum>")

def parse_analog(arg):
    """Helper to parse wafer coordinate"""
    return AnalogOnHICANN(Enum(int(arg)))

class FPGAAction(argparse.Action):
    def __init__(self, option_strings, dest, **kwargs):
        super(FPGAAction, self).__init__(
            option_strings, dest, nargs=None, type=int, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        wafer = namespace.wafer
        if not isinstance(wafer, Wafer):
            raise argparse.ArgumentTypeError(
                "--wafer is needed before --fpga")
        fpga = FPGAGlobal(FPGAOnWafer(values), wafer)
        value = gridLookupDNCGlobal(fpga, DNCOnFPGA(0))
        setattr(namespace, self.dest, value)


def add_default_coordinate_options(parser):
    group = parser.add_argument_group('HICANN selection:')
    group.add_argument(
        '--w', '--wafer', action='store', required=False,
        type=parse_wafer, dest='wafer',
        help='specify the Wafer system to use, choose 0 on vertical setups')
    group.add_argument(
        '--hicann', action='store', required=False,
        type=parse_hicann, metavar='<enum>|<x>,<y>', dest='hicann',
        help='specify the HICANN on the wafer system to use')
    group.add_argument(
        '--r', '--reticle', action='store', required=False,
        type=parse_reticle, metavar='<reticle>,<hicann>', dest='hicann',
        help='specify reticle and hicann on dnc to use: -r 10,0')
    group.add_argument(
        '--a', '--aout', action='store', required=False,
        type=parse_analog, dest='analog', nargs="+",
        help='specify analog output to use')

def add_fpga_coordinate_options(parser):
    group = parser.add_argument_group('FPGA/DNC selection:')
    group.add_argument(
        '--w', '--wafer', action='store', required=False,
        type=parse_wafer, dest='wafer',
        help='specify the Wafer system to use, choose 0 on vertical setups')
    group.add_argument(
        '--fpga', action=FPGAAction, required=False,
        metavar='<enum>', dest='dnc',
        help='specify FPGAOnWafer')
    group.add_argument(
        '--dnc', action='store', required=False,
        type=parse_dnc, metavar='<enum>|<x>,<y>', dest='dnc',
        help='specify DNCOnWafer')
    group.add_argument(
        '--a', '--aout', action='store', required=False,
        type=parse_analog, dest='analog', nargs="+",
        help='specify analog output to use')


def to_level(level, default=pylogging.LogLevel.ALL):
    """parse level string"""
    if isinstance(level, pylogging.LogLevel):
        return level
    else:
        return pylogging.LogLevel.toLevel(level, default)

# TODO add logfile support
def add_logger_options(parser, default_level=pylogging.LogLevel.INFO):

    def parse_log_channel(arg):
        """Helper to parse logchannel, given as <name>:<level>"""
        try:
            channel, level_string = arg.split(":")
            return channel, to_level(level_string)
        except Exception as err:
            print(err)
            raise argparse.ArgumentTypeError(
                "Please provide --logchannel <name>:<level>")

    default_level = to_level(default_level)
    to_level_with_default = lambda x: to_level(x, default_level)

    group = parser.add_argument_group('logger settings:')
    group.add_argument(
        '--loglevel', action='store', required=False,
        type=to_level_with_default, default=default_level,
        help="Basic log level, if the string cannot be understood,"
             " " + default_level.toString() + " is used")
    group.add_argument(
        '--logchannel', action='append', required=False, default=[],
        type=parse_log_channel, metavar='<channelname>:<level>',
        help='specify level for certain logchannel. E.g. --logchannel '
             'main:INFO')
    group.add_argument(
        '--logdate_format', action='store', required=False,
        default='absolute',
        help='specify dateformat, values are NULL, RELATIVE, ABSOLUTE, '
             'DATE, ISO8601')
    group.add_argument(
        '--logcolor', action='store_true', required=False, default=True)
    group.add_argument(
        '--loglocation', action='store_true', required=False, default=False)
    group.add_argument(
        '--logfile', default=None, help="Log to file")
    group.add_argument(
        '--perflogpath', action='store', required=False,
        help="Specify path where perf logger output will be stored")


def init_logger(default_level=pylogging.LogLevel.INFO, defaults=tuple(),
                add_help=False):
    """Initialize logging system

    Arguments:
        default_level: default log level for the logging system
        args: list of tuples for channel default values
        add_help: [bool] print help when called with --help, set to True
                         if you don't have an own parser.

    Example:
        init_logger(args, [('sthal', 'INFO'), ('halbe', 'WARN')])
    """

    parser = argparse.ArgumentParser(
        description='PSP visualization tool', add_help=add_help)
    add_logger_options(parser, default_level)
    parser_args, _ = parser.parse_known_args()

    pylogging.reset()
    pylogging.default_config(
        level=parser_args.loglevel,
        fname=parser_args.logfile if parser_args.logfile else "",
        print_location=parser_args.loglocation,
        color=parser_args.logcolor,
        date_format=parser_args.logdate_format)

    for name, level in defaults:
        pylogging.set_loglevel(pylogging.get(name), to_level(level))
    for name, level in parser_args.logchannel:
        pylogging.set_loglevel(pylogging.get(name), to_level(level))

def folder(value):
    """
    Give as type argument in argparse to check that a folder exists
    and is writeable, e.g:
        parser.add_argument('--results', action='store', type=folder,)
    """
    if not os.path.isdir(value):
        try:
            os.makedirs(value)
        except Exception as err:
            msg = "Couldn't created output folder {}: {}".format(value, err)
            raise argparse.ArgumentTypeError(msg)
    if not os.access(value, os.R_OK | os.W_OK | os.X_OK):
        raise argparse.ArgumentTypeError(
            "{0} is not accessible".format(value))
    return value
