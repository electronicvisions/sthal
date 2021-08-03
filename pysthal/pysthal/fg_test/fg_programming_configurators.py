"""
HICANN configurators that are used for the automated floating gate tests.
"""

import pysthal
import pyhalbe
from pyhalco_common import Enum
import pyhalco_hicann_v2 as C
import pylogging

from copy import deepcopy


class TestConfigurator(pysthal.HICANNConfigurator):
    """
    Base class for all test configurators.  This adds the function
    `get_most_recent_write_results` to query the error flags that were
    read out from the floating gate controller during the latest write
    cycle.
    Subclassesa must implement `_config_floating_gates`.
    """
    def __init__(self):
        super(TestConfigurator, self).__init__()
        self._write_results_list = []

    def get_most_recent_write_results(self):
        return self._write_results_list

    def config_floating_gates(self, handle, hicann):
        self._write_results_list = []
        self._config_floating_gates(handle, hicann)

    def _config_floating_gates(self, handle, hicann):
        raise NotImplementedError(type(self))

    def config_fpga(self, *args):
        pass

    def program_rows_zero_neuron_values(self, handle, hicann, rows):
        fgc = hicann.floating_gates

        num_passes = fgc.getNoProgrammingPasses()
        for step in range(num_passes):
            fgconfig = fgc.getFGConfig(Enum(step))

            logger = pylogging.get("test_fg_writing_precision")
            logger.debug("using FGConfig:")
            logger.debug(str(fgconfig))

            for block in C.iter_all(C.FGBlockOnHICANN):
                pyhalbe.HICANN.set_fg_config(handle, block, fgconfig)

            for row_idx, row in enumerate(rows):
                writeDown = fgconfig.writeDown
                data = [deepcopy(fgc[blk].getFGRow(row[int(blk.toEnum())]))
                        for blk in C.iter_all(C.FGBlockOnHICANN)]

                for i in range(len(data)):
                    for nrn_c in C.iter_all(C.NeuronOnFGBlock):
                        data[i].setNeuron(nrn_c, 0)

                res = pyhalbe.HICANN.set_fg_row_values(
                    handle, row, data, writeDown)

                logger = pylogging.get("test_fg_writing_precision")
                logger.info("write result for step {} row {}".format(step, row_idx))

                res_picklable = dict()
                for key in list(res.keys()):
                    value = res[key]
                    value_pick = [i.get_cell() for i in value]
                    res_picklable[key] = value_pick

                logger.info(res_picklable)

                self._write_results_list.append(
                    dict(
                        step=step,
                        row=row_idx,
                        result=res_picklable,
                        writeDown=writeDown,
                        fgconfig=fgconfig))

    def program_rows(self, handle, hicann, rows):
        fgc = hicann.floating_gates

        num_passes = fgc.getNoProgrammingPasses()
        for step in range(num_passes):
            fgconfig = fgc.getFGConfig(Enum(step))

            logger = pylogging.get("test_fg_writing_precision")
            logger.debug("using FGConfig:")
            logger.debug(str(fgconfig))

            for block in C.iter_all(C.FGBlockOnHICANN):
                pyhalbe.HICANN.set_fg_config(handle, block, fgconfig)

            for row_idx, row in enumerate(rows):
                writeDown = fgconfig.writeDown
                data = [fgc[blk].getFGRow(row[int(blk.toEnum())])
                        for blk in C.iter_all(C.FGBlockOnHICANN)]

                res = pyhalbe.HICANN.set_fg_row_values(
                    handle, row, data, writeDown)

                logger = pylogging.get("test_fg_writing_precision")
                logger.info("write result for step {} row {}".format(step, row_idx))

                res_picklable = dict()
                for key in list(res.keys()):
                    value = res[key]
                    value_pick = [i.get_slave_answer_data() for i in value]
                    res_picklable[key] = value_pick

                logger.info(res_picklable)

                self._write_results_list.append(
                    dict(
                        step=step,
                        row=row_idx,
                        result=res_picklable,
                        writeDown=writeDown,
                        fgconfig=fgconfig))


class SequentialTestConfigurator(TestConfigurator):
    """
    A test configurator that writes all floating gates sequentially.
    """
    def config(self, _, hicann_handle, hicann_data):
        pyhalbe.HICANN.init(hicann_handle, False)
        self.config_floating_gates(hicann_handle, hicann_data)

    def _config_floating_gates(self, handle, hicann):
        all_rows = tuple(row for row in C.iter_all(C.FGRowOnFGBlock))

        write_rows = [[row]*4 for row in all_rows]
        rows = write_rows

        self.program_rows(handle, hicann, rows)


class IconvLowTestConfigurator(TestConfigurator):
    """
    A test configurator that writes I_conv to zero, writes all
    other parameters, then writes I_conv.
    """
    def _config_floating_gates(self, handle, hicann):
        all_rows = tuple(row for row in C.iter_all(C.FGRowOnFGBlock))
        int_op_rows = [[all_rows[1]] * 4]

        # left: 3 Iconvi, 17 Iconvx
        # right: 1 Iconvi 3 Iconvx

        iconv_rows = [[all_rows[3],  all_rows[1], all_rows[3],  all_rows[1]],    #pylint: disable=C0326
                      [all_rows[17], all_rows[3], all_rows[17], all_rows[3]]]

        other_rows = [[row] * 4
                      for idx, row in enumerate(all_rows)
                      if idx != 1 and idx != 3 and idx != 17]
        other_rows += [[all_rows[1], all_rows[17], all_rows[1], all_rows[17]]]

        self.program_rows(handle, hicann, int_op_rows)
        self.program_rows_zero_neuron_values(handle, hicann, iconv_rows)
        self.program_rows(handle, hicann, other_rows)
        self.program_rows(handle, hicann, iconv_rows)
