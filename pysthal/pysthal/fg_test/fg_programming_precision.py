"""
Utility functions for the floating gate test script.
"""

import pandas
import unittest
import pyhalbe
from pyhalbe.HICANN import shared_parameter, neuron_parameter
from pyhalco_common import Enum
import pyhalco_hicann_v2 as C
import pylogging


class ControllerResultNotAvailable(object):
    pass


class TestResult(object):
    """
    This class holds the test result of a single pass of measurements.
    """
    def __init__(self):
        self._fg_results = None
        self._result_list = []

    def set_fg_controller_result_not_available(self):
        """
        Use this method to indicate that the FG controller
        result is not available.
        """
        self._fg_results = ControllerResultNotAvailable()

    def set_fg_controller_result(self, fg_results):
        """
        Set the result of the last write cycle that was returned
        by the floating gate controller.
        """
        if self._fg_results is not None:
            raise RuntimeError("fg controller results already set")

        self._fg_results = fg_results

    def add_param_result(self, neuron_c, parameter, v_mean, v_std):
        """
        Add the result for the given `neuron_c` (NeuronOnHICANN) and
        `parameter` (shared_parameter or neuron_parameter).  The
        result is given by the mean (v_mean) and standard deviation of
        the measured voltage.
        """
        self._result_list.append((neuron_c, type(parameter), parameter, v_mean, v_std))

    def get_param_result(self):
        """
        Returns a pandas.DataFrame with the recorded measurements.
        """
        result = pandas.DataFrame(
            self._result_list,
            columns=('neuron_c', 'parameter_type', 'parameter', 'mean_v', 'std_v'))
        return result

    def get_param_result_raw(self):
        return self._result_list

    def get_fg_controller_result(self):
        return self._fg_results


class TestPattern(object):
    """
    Holds data of floating-gate test pattern.
    """
    def __init__(self):
        self.fg_block_values = dict()
        for blk in C.iter_all(C.FGBlockOnHICANN):
            self.fg_block_values[blk] = pyhalbe.HICANN.FGBlock(blk)

        self.modified_values = []

    def get_block(self, blk):
        """
        Return the data for the block given by `blk` (FGBlockOnHICANN).
        The result is a pyhalbe.HICANN.FGBlock
        """
        return self.fg_block_values[blk]

    def set_value(self, nrn, parameter, value, add_to_value_list=True):
        """
        Set the value of the floating gate which controls the parameter
        `parameter` (either shared_parameter or neuron_parameter) for
        neuron `nrn` (NeuronOnHICANN).

        `add_to_value_list` (bool) determines whether the list of modified
        values is updated.
        """
        if isinstance(parameter, neuron_parameter):
            self.fg_block_values[nrn.toNeuronFGBlock()].setNeuron(
                nrn.toNeuronFGBlock(),
                nrn.toNeuronOnFGBlock(),
                parameter,
                value)
        elif isinstance(parameter, shared_parameter):
            self.fg_block_values[nrn.toSharedFGBlockOnHICANN()].setShared(
                nrn.toSharedFGBlockOnHICANN(),
                parameter,
                value)
        else:
            raise ValueError()

        if add_to_value_list:
            # Cannot use set() because mixed shared_parameter and
            # neuron_parameter can not be sorted in self.get_modified_values
            if (nrn, parameter) not in self.modified_values:
                self.modified_values.append((nrn, parameter))

    def add_nrn_pattern(self, num_nrns, start_nrn, lookup_func,
                        parameter=neuron_parameter.V_t):
        """
        Add a pattern for the given `parameter` (either shared_parameter
        or neuron_parameter) over several neurons. The number of neurons
        is given by `num_nrns`, starting with `start_nrn`. `lookup_func`
        maps a value in [0; 1] to the value of the floating gate. The value
        may be a floating point number.
        """
        for idx in xrange(start_nrn, start_nrn + num_nrns):
            nrn = C.NeuronOnHICANN(Enum(idx))
            value = int(lookup_func((idx - start_nrn) / float(num_nrns)))

            self.set_value(nrn, parameter, value)

    def get_modified_values(self):
        """
        Return the list of modified values
        """
        return self.modified_values

    def set_essential_shared(self):
        """
        Set the parameters `V_m` and `int_op_bias` for neurons on all four
        blocks to 'safe' values.
        """
        nrn_list = [
            C.NeuronOnHICANN(Enum(254)),
            C.NeuronOnHICANN(Enum(255)),
            C.NeuronOnHICANN(Enum(510)),
            C.NeuronOnHICANN(Enum(511))]

        for nrn in nrn_list:
            self.set_value(nrn, shared_parameter.int_op_bias, 1023, False)
            self.set_value(nrn, shared_parameter.V_m, 100, False)


    def add_nrn_param_pattern(self, lookup_func):
        """
        Add a pattern over all neuron parameters of four neurons (one on
        each block) using the lookup function `lookup_func`
        """
        nrns = [
            C.NeuronOnHICANN(Enum(204)),
            C.NeuronOnHICANN(Enum(205)),
            C.NeuronOnHICANN(Enum(460)),
            C.NeuronOnHICANN(Enum(461))]

        neuron_param_list = [
            neuron_parameter.E_l,
            neuron_parameter.E_syni,
            neuron_parameter.E_synx,
            neuron_parameter.I_bexp,
            neuron_parameter.I_convi,
            neuron_parameter.I_convx,
            neuron_parameter.I_fire,
            neuron_parameter.I_gl,
            neuron_parameter.I_gladapt,
            neuron_parameter.I_intbbi,
            neuron_parameter.I_intbbx,
            neuron_parameter.I_pl,
            neuron_parameter.I_radapt,
            neuron_parameter.I_rexp,
            neuron_parameter.I_spikeamp,
            neuron_parameter.V_exp,
            neuron_parameter.V_syni,
            neuron_parameter.V_syntci,
            neuron_parameter.V_syntcx,
            neuron_parameter.V_synx,
            neuron_parameter.V_t,
            neuron_parameter.V_convoffi,
            neuron_parameter.V_convoffx,
            ]

        idx = 0
        value = 0
        for nrn in nrns:
            for param in neuron_param_list:
                idx += 1
                value = int(lookup_func(float(idx) / (len(nrns) * len(neuron_param_list))))
                self.set_value(nrn, param, value)

    def add_shared_param_pattern(self, lookup_func):
        """
        Add a pattern over all shared parameters of four neurons (one on
        each block) using the lookup function `lookup_func`

        Note: Because floating gate cells are addressed by (neuron,
        parameter) all neurons are equivalent for shared parameters
        as long as they obtain their parameter values from the same
        block.
        """
        nrns = [
            C.NeuronOnHICANN(Enum(254)),
            C.NeuronOnHICANN(Enum(255)),
            C.NeuronOnHICANN(Enum(510)),
            C.NeuronOnHICANN(Enum(511))]

        shared_param_list = [
            shared_parameter.V_reset,
            shared_parameter.int_op_bias,
            shared_parameter.V_dllres,
            shared_parameter.V_bout,
            shared_parameter.V_bexp,
            shared_parameter.V_fac,
            shared_parameter.I_breset,
            shared_parameter.V_dep,
            shared_parameter.I_bstim,
            shared_parameter.V_thigh,
            shared_parameter.V_gmax3,
            shared_parameter.V_tlow,
            shared_parameter.V_gmax0,
            shared_parameter.V_clra,
            shared_parameter.V_clrc,
            shared_parameter.V_gmax1,
            shared_parameter.V_stdf,
            shared_parameter.V_gmax2,
            shared_parameter.V_m,
            shared_parameter.V_bstdf,
            shared_parameter.V_dtc,
            shared_parameter.V_br,
            shared_parameter.V_ccas,
            ]

        logger = pylogging.get("test_fg_writing_precision")

        idx = 0
        value = 0
        for nrn in nrns:
            for param in shared_param_list:
                # NOTE: not all parameters available on all quads
                value = int(lookup_func(float(idx) / (len(nrns) * (len(shared_param_list) - 2))))

                try:
                    self.set_value(nrn, param, value)
                except IndexError:
                    # NOTE: Skip parameters which are not present on this block
                    logger.info("Skipping parameter {} for neuron {}".format(
                        str(param), str(nrn)))
                    continue
                idx += 1

    def zero_all_iconv(self):
        """
        Set I_conv to 0 for all neurons
        """
        for nrn in C.iter_all(C.NeuronOnHICANN):
            self.set_value(nrn, neuron_parameter.I_convx, 0, False)
            self.set_value(nrn, neuron_parameter.I_convi, 0, False)

