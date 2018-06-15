#!/usr/bin/env python
# -*- encoding: utf-8 -*-

import json
import os
import sys
import time
import unittest

import pandas

import Coordinate
import pyhalbe
import pysthal


NOISE_LIMIT = 0.010
DAC_TO_VOLTAGE_MARGIN = 0.080
FILE_TRACES = 'traces.hdf'
FILE_DATA = 'data.json'
FILE_PLOT = 'plot.png'


def generate_plot(filename, traces, data, result):
    """
    Generate a overview plot of the recorded raw traces
    """
    import matplotlib as mpl
    mpl.use('Agg')
    import matplotlib.pyplot as plt

    wafer = data['wafer']
    dnc = data['dnc']
    adcs = sorted(data['adcs'])

    fig, axes = plt.subplots(2, 8, figsize=(30, 9), sharex=True, sharey=True)
    plt_rows = {h: axes[:, ii] for ii, h in
                enumerate(sorted(set(x[2] for x in adcs)))}

    print result
    for wafer, dnc, hicann, analog, adc, channel, errors in adcs:
        axis = plt_rows[hicann][analog]
        status = []

        if errors:
            for _, dac in errors:
                pos = 1.2 / 1023 * dac
                axis.text(0.01, pos, 'ADC ERROR for DAC[{}]'.format(dac),
                          color='red',
                          horizontalalignment='left',
                          verticalalignment='top',
                          transform=axis.transAxes)
        if (wafer, dnc, hicann, analog) in traces:
            plt_data = traces[(wafer, dnc, hicann, analog)]
            noises = []
            for column in plt_data.columns:
                (line, ) = axis.plot(plt_data[column], label=column)
                axis.axhline(column / 1023.0 * 1.8,
                             color=line.get_color(), linestyle='--')
                noise = plt_data[column].std()
                noises.append("\nDAC[{}]: {:.3f}V".format(column, noise))

            axis.text(0.01, 0.99, "NOISE: " + "".join(noises),
                      horizontalalignment='left',
                      verticalalignment='top',
                      transform=axis.transAxes)

            tmp = result[(result['hicann'] == hicann) &
                         (result['analog'] == analog)]
            if any(tmp['fg_error']):
                status.append('wrong FG value')
            if any(tmp['noise_error']):
                status.append('too noisy')
            if not status:
                status.append('OK')

        axis.set_title("hicann: {}, analog: {}\n{} channel: {}\n{}".format(
            hicann, analog, adc, channel, " & ".join(status)))
        axis.set_ylim(-0.1, 1.4)
        axis.set_xlabel("Samples")
        axis.set_ylabel("Membrane [V]")
        axis.legend(loc='upper right')
    fig.tight_layout()
    plt.savefig(filename)


def load_and_generate_plot(folder, wafer, dnc):
    """
    Regenerate the plot from stored traces
    """
    trace_file = os.path.join(folder, FILE_TRACES)
    if not os.path.exists(trace_file):
        print "No traces found at '{}'".format(trace_file)
        return
    traces = pandas.read_hdf(trace_file, key='traces')
    result = pandas.read_hdf(trace_file, key='result')
    with open(os.path.join(folder, FILE_DATA)) as infile:
        data = json.load(infile)
    plt_file = os.path.join(folder, FILE_PLOT)
    generate_plot(plt_file, traces, data, result)


class SwitchAoutConfigurator(pysthal.HICANNConfigurator):
    """
    Connects <param> FG cell to analog out
    """
    def __init__(self, fg_cell):
        pysthal.HICANNConfigurator.__init__(self)
        self.fg_cell = fg_cell

    def config_fpga(self, *args):
        pass

    def config(self, fpga, handle, hicann):
        for fg_block in Coordinate.iter_all(Coordinate.FGBlockOnHICANN):
            pyhalbe.HICANN.set_fg_cell(handle, fg_block, self.fg_cell)
        self.config_analog_readout(handle, hicann)
        self.flush_hicann(handle)


class UpdateFGRowConfigurator(pysthal.HICANNConfigurator):
    """
    Update the floating gate
    """
    def __init__(self, fg_row):
        pysthal.HICANNConfigurator.__init__(self)
        self.fg_row = fg_row

    def config_fpga(self, *args):
        pass

    def config(self, fpga, handle, hicann):
        fg = hicann.floating_gates
        for fgpass in range(fg.getNoProgrammingPasses()):
            cfg = fg.getFGConfig(Coordinate.Enum(fgpass))
            for block in Coordinate.iter_all(Coordinate.FGBlockOnHICANN):
                pyhalbe.HICANN.set_fg_config(handle, block, cfg)
            pyhalbe.HICANN.set_fg_row_values(
                handle, self.fg_row, fg, cfg.writeDown)

        # Update for VerifyConfiguration
        self.config_fg_stimulus(handle, hicann)
        self.flush_hicann(handle)


class TestRecticleAout(unittest.TestCase):
    """
    Test suite to test all analog output on all HICANNs on a single reticle
    and to check the noise levels and ADC connections
    The test protocol is:

    1. Fully configure HICANN with all floating gates set to zero
    2. For each HICANN:
    2.1. Read 2 floating gate cells on 2 blocks -> check if they are low
    2.2. Update the floating gate row to 512
    2.3. Reread the floating gate cells -> check for change
    """

    # Fairly random choosen ;)
    ROW = Coordinate.FGRowOnFGBlock(4)
    CELL = Coordinate.FGCellOnFGBlock(Coordinate.X(30), ROW)
    RECORDING_TIME = 1e-5

    def toHICANNOnWafer(self, hicann_on_dnc):
        return hicann_on_dnc.toHICANNOnWafer(self.DNC)

    def reset_hicann_config(self):
        """
        Set hicann configuration to default values, with zeroed floating gates
        """
        for hicann_c in self.hicanns:
            hicann = self.wafer[hicann_c]
            hicann.clear()
            for block in Coordinate.iter_all(Coordinate.FGBlockOnHICANN):
                fgblock = hicann.floating_gates[block]
                for cell in Coordinate.iter_all(Coordinate.FGCellOnFGBlock):
                    fgblock.setRaw(cell, 0)

    def save_data(self, traces=None, result=None):
        """
        Store data for the test-wafer-out-overview-plot.py evaluation script
        """
        if self.DATA_FOLDER is None:
            return None, None

        adcs = self.data_adcs
        data = {
            'wafer': int(self.WAFER),
            'dnc': int(self.DNC.id()),
            'dnc_xy': [int(self.DNC.x()), int(self.DNC.y())],
            'completed': result is not None,
            'adcs': [k + adcs[k] for k in sorted(adcs.keys())],
            'starttime': self.starttime,
            'runner_outsuffix': getattr(self, 'runner_outsuffix', None),
        }
        with open(os.path.join(self.DATA_FOLDER, FILE_DATA), 'w') as out:
            json.dump(data, out)

        if traces is not None and result is not None:
            with pandas.HDFStore(os.path.join(self.DATA_FOLDER, FILE_TRACES),
                                 'w', complevel=9, complib='blosc') as store:
                store['traces'] = traces
                store['result'] = result

        return data

    def adc_key(self, hicann_c, analog):
        """
        Composes wafer, dnc, hicann and analog channel into a tuple, to
        uniquely identify a analog channel on the wafer. This discards halbe
        types and only contains integers.
        """
        return (
            int(self.WAFER),
            int(self.DNC.id()),
            int(hicann_c.id()),
            int(analog),
        )

    def read_trace(self, hicann, analog, dac):
        """
        Read the ADC, dac reprents the expected DAC value.
        """
        adc_key = self.adc_key(hicann.index().toHICANNOnWafer(), analog)
        trace_key = adc_key + (dac, )
        try:
            adc = hicann.analogRecorder(analog)
            adc.setRecordingTime(self.RECORDING_TIME)
            adc.record()
            return {trace_key: adc.trace()}
        except RuntimeError as exc:
            return {trace_key: exc}

    def hicann_out_test(self, hicann):
        """
        Execute AnaRM test for a single HICANN:
            1. Read two floating gate cells, expected value is 0
            2. Update two floating gate rows with different values
            3. Read updated rows
        """
        data = {}

        hicann.analog.set_fg_left(Coordinate.AnalogOnHICANN(0))
        hicann.analog.set_fg_right(Coordinate.AnalogOnHICANN(1))

        fg_blocks = {
            Coordinate.AnalogOnHICANN(0): (
                Coordinate.FGBlockOnHICANN(Coordinate.Enum(0)), 300),
            Coordinate.AnalogOnHICANN(1): (
                Coordinate.FGBlockOnHICANN(Coordinate.Enum(3)), 600),
        }

        self.wafer.configure(UpdateFGRowConfigurator(self.ROW))
        self.wafer.configure(SwitchAoutConfigurator(self.CELL))

        for analog in Coordinate.iter_all(Coordinate.AnalogOnHICANN):
            data.update(self.read_trace(hicann, analog, 0))

        for analog in Coordinate.iter_all(Coordinate.AnalogOnHICANN):
            block, dac = fg_blocks[analog]
            fgblock = hicann.floating_gates[block]
            for xvalue in Coordinate.iter_all(Coordinate.FGCellOnFGBlock.x_type):
                cell = Coordinate.FGCellOnFGBlock(xvalue, self.ROW)
            for cell in Coordinate.iter_all(Coordinate.FGCellOnFGBlock):
                fgblock.setRaw(cell, dac)

        self.wafer.configure(UpdateFGRowConfigurator(self.ROW))
        self.wafer.configure(SwitchAoutConfigurator(self.CELL))

        for analog in Coordinate.iter_all(Coordinate.AnalogOnHICANN):
            _, dac = fg_blocks[analog]
            data.update(self.read_trace(hicann, analog, dac))

        return data

    @staticmethod
    def evaluate_traces(data):
        """
        Extract traces and ADC errors from data and evaluetes them.
        The mean is compared to the target fg value and the standard deviation
        is checked agains a threshhold.

        Returns pandas.DataFrame with columns:
            wafer, dnc, hicann, analog: coordinates
            DAC: target dac value
            V: target dac value in volts
            mean: mean of trace, NaN in case of ADC error
            std: standard deviation of trace, NaN in case of ADC error
            adc error: error message from ADC, emtpy string if None
            fg error: error message in case of from fg value, emtpy string if none
            noise error: error message in case of to high noise, emtpy string if none
        """
        if not data:
            raise RuntimeError("No traces recorded, this should not happen :(")

        traces = pandas.DataFrame(
            {k: v
             for k, v in data.iteritems()
             if not isinstance(v, Exception)})
        adc_errors = pandas.Series(
            {k: "ADC Error DAC[{}]: {}".format(k[-1], str(v))
             for k, v in data.iteritems()
             if isinstance(v, Exception)})

        result = pandas.concat(axis=1, objs={
            'adc_error': adc_errors,
            'mean': traces.mean(),
            'std': traces.std()
        })
        result.index.names = ['wafer', 'dnc', 'hicann', 'analog', 'DAC']
        result.reset_index(inplace=True)
        result['adc_error'].fillna("", inplace=True)

        result['V'] = result['DAC'] * 1.8 / 1023

        def format_fg_error(row):
            """nicely format an fg_error"""
            lower_bound = row['V'] - DAC_TO_VOLTAGE_MARGIN
            upper_bound = row['V'] + DAC_TO_VOLTAGE_MARGIN
            if not lower_bound < row['mean'] < upper_bound:
                fg_err_msg = ("FG value error: DAC[{DAC}] "
                              "{mean:.3f}V not in [{:.3f}V, {:.3f}V]")
                return fg_err_msg.format(lower_bound, upper_bound, **row)
            else:
                return ""

        def format_noise_error(row):
            if row['std'] > NOISE_LIMIT:
                noise_err_msg = ("Noise error: DAC[{DAC}] "
                                 "std = {std:.3f}V larger than {:.3f}V")
                return noise_err_msg.format(NOISE_LIMIT, **row)
            else:
                return ""

        result['fg_error'] = result.apply(format_fg_error, axis='columns')
        result['noise_error'] = result.apply(format_noise_error, axis='columns')

        error_columns = ['adc_error', 'fg_error', 'noise_error']
        result['error_count'] = result[error_columns].apply(
            lambda row: sum(bool(v) for v in row), axis='columns')
        result.sort_values(by=['hicann', 'DAC', 'analog'], inplace=True)
        return traces, result

    @classmethod
    def setUpClass(cls):
        cls.starttime = time.time()

    def setUp(self):
        """
        Set up the unit test:

        A full reticle is allocated and complelty configured with zeroed
        floating gates.
        """
        if None in (self.WAFER, self.DNC):
            raise unittest.SkipTest("No DNC selected, skipping hardware test")
            return

        self.hicanns = [
            self.toHICANNOnWafer(hicann_on_dnc)
            for hicann_on_dnc in Coordinate.iter_all(Coordinate.HICANNOnDNC)]

        self.data_adcs = {}

        # Dump data in case of an error in setup
        self.save_data()

        self.wafer = pysthal.Wafer(self.WAFER)
        self.reset_hicann_config()

        db = pysthal.MagicHardwareDatabase()
        self.wafer.connect(db)

        for hicann_c in self.hicanns:
            hicann = self.wafer[hicann_c]
            for analog in Coordinate.iter_all(Coordinate.AnalogOnHICANN):
                key = self.adc_key(hicann_c, analog)
                try:
                    cfg = hicann.getADCConfig(analog)
                    self.data_adcs[key] = (
                        cfg.coord.value(), cfg.channel.value(), [])
                except RuntimeError:
                    self.data_adcs[key] = (None, None, [])

        self.wafer.configure(pysthal.HICANNConfigurator())

    def test_analog_readout(self):
        """Test HICANN AnaRM connections"""

        test_raw_data = {}
        for hicann_c in self.hicanns:
            self.reset_hicann_config()
            test_raw_data.update(self.hicann_out_test(self.wafer[hicann_c]))

        cfg = pysthal.VerifyConfigurator()  # Just read back, don't update values on hardware
        self.wafer.configure(cfg)

        traces, results = self.evaluate_traces(test_raw_data)

        # The results are not saved in the tearDown function, because every
        # error before this point is a configuration error or a error in the
        # test. In this case we don't want to save partial results but fail
        # completely.
        if self.DATA_FOLDER is not None:
            data = self.save_data(traces, results)
            plt_file = os.path.join(self.DATA_FOLDER, FILE_PLOT)
            generate_plot(plt_file, traces, data, results)

        errors = []
        msg = "HICANN {} Analog {} DAC[{}]: {:.3f}V +- {:.3f}V"

        # Collect error message from results
        for row in results.itertuples():
            if row.error_count > 0:
                errors.append(
                    "Error for HICANN {} Analog {}".format(
                        row.hicann, row.analog))
                for err in (row.adc_error, row.fg_error, row.noise_error):
                    if err:
                        errors.append("   - " + err)
            print msg.format(
                row.hicann, row.analog, row.DAC, row.mean, row.std)
        if cfg.error_count() > 0:
            errors.append(str(cfg))
        if errors:
            self.fail("\n".join(errors))

    @classmethod
    def main(cls, *was_args, **was_kwargs):
        import argparse
        from pysthal.command_line_util import add_fpga_coordinate_options
        from pysthal.command_line_util import add_logger_options
        from pysthal.command_line_util import folder
        from pysthal.command_line_util import init_logger

        init_logger('INFO')

        parser = argparse.ArgumentParser(
                description='SthalHWTest: %s' % cls.__name__)
        add_fpga_coordinate_options(parser)
        parser.add_argument(
            '--hwdb', type=str, default=None,
            help="full path to hardware database")
        parser.add_argument(
            '--data-output-dir', type=folder, default=None,
            help="Store outputs in this folder")
        parser.add_argument(
            '--replot', action='store_true',
            help='do not execute any test, just recreate the plot')
        add_logger_options(parser)

        args, argv = parser.parse_known_args()
        argv.insert(0, sys.argv[0])

        if args.hwdb:
            settings = pysthal.Settings.get()
            settings.yaml_hardware_database_path = args.hwdb
            print "using non-default hardware database {}".format(args.hwdb)

        if args.data_output_dir:
            output_dir = os.path.join(
                args.data_output_dir,
                'wafer{}_dnc{:0>2}'.format(int(args.wafer), int(args.dnc.id())))
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)
        else:
            output_dir = None

        cls.WAFER = args.wafer
        cls.DNC = args.dnc
        cls.DATA_FOLDER = output_dir

        if args.replot:
            load_and_generate_plot(output_dir, args.wafer, args.dnc)
            return

        test_runner = None
        if output_dir:
            from xmlrunner import XMLTestRunner
            test_runner = XMLTestRunner(output=output_dir)
            cls.runner_outsuffix = test_runner.outsuffix

        unittest.main(argv=argv, testRunner=test_runner, *was_args, **was_kwargs)

if __name__ == '__main__':
    TestRecticleAout.main()
