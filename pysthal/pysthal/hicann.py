'''Stateful pyhalbe.HICANN functions'''
import numpy as np
import pyhalbe

class CoordinateIterator(object):
    def __init__(self, coordinate):
        self.coordinate = coordinate
        self.value = coordiCoordinate.NeuronOnHICANNnate(coordinate.begin)

    def next(self):
        if self.value == coordinate.end:
            raise StopIteration
        v = self.coordinate(self.value)
        self.value += 1
        return v

    @staticmethod
    def add(coordinate):
        if hasattr(coordinate, 'all'):
            raise RuntimeError("OHHH NOOOOO")
        coordinate.iter = staticmethod(lambda: CoordinateIterator(coordinate))

CoordinateIterator.add(pyhalbe.Coordinate.AnalogOnHICANN)
CoordinateIterator.add(pyhalbe.Coordinate.NeuronOnHICANN)
CoordinateIterator.add(pyhalbe.Coordinate.QuadOnHICANN)


class HICANNData(object):
    def __init__(self):
        self.analogs = [pyhalbe.HICANN.Analog() for ii
                in range(pyhalbe.Coordinate.AnalogOnHICANN.size)]
        self.quads = [pyhalbe.HICANN.NeuronQuad() for ii
                in range(pyhalbe.Coordinate.QuadOnHICANN.size)]
        self.neuron_config = pyhalbe.HICANN.NeuronConfig()
        self.fg_control = pyhalbe.FGControl()

    def __getitem__(self, coord):
        if isinstance(coord, pyhalbe.Coordinate.AnalogOnHICANN):
            return self.quads[int(coord)]
        elif isinstance(neuron, pyhalbe.Coordinate.NeuronOnHICANN):
            return self.quads[neuron.toQuadOnHICANN()][neuron.toNeuronOnQuad()]
        elif isinstance(neuron, pyhalbe.Coordinate.QuadOnHICANN):
            return self.quads[int(coord)]
        else:
            raise AttributeError

    def all_neurons(self):
        n = [self.get_neuron(n) for n in pyhalbe.Coordinate.NeuronOnHICANN.all()]

    def send(self, h, fpga, flush=False):
        pyhalbe.HICANN.set_neuron_config(h, self.neuron_config)
        for n in pyhalbe.Coordinate.QuadOnHICANN.all():
            pyhalbe.HICANN.set_denmem_quad(h, q, self[q])
        for a in pyhalbe.Coordinate.AnalogOnHICANN.all():
            pyhalbe.HICANN.set_analog(a, self[a])
        pyhalbe.HICANN.set_neuron_config(h, self.neuron_config)
        pyhalbe.HICANN.set_fg_values(h, self.fg_control.extractFullFGBlocks)

        if flush:
            pyhalbe.HICANN.flush(h)

    def read(self, h, fpga, flush=False):
        self.neuron_config = pyhalbe.HICANN.set_neuron_config(h)
        for n in pyhalbe.Coordinate.QuadOnHICANN.all():
            self.quads[int(q)] = pyhalbe.HICANN.get_denmem_quad(h, q)
        for a in pyhalbe.Coordinate.AnalogOnHICANN.all():
            self.analogs[int(a)] = pyhalbe.HICANN.get_analog(a)
        self.fg_control = pyhalbe.FGControl()



    def check(self):
        pass


class HICANN(HICANNData):
    def __init__(self):
        super(HICANN, self).__init__()

    def enable_neuron_aout(self, neuron, exclusive=True):
        assert isinstance(neuron, pyhalbe.Coordinate.NeuronOnHICANN)
        if exclusive:
            y = neuron.y()
            x = int(neuron.x()) % 2
            for x in range(x, neuron.x_type.max, 2):
                n = pyhalbe.Coordinate.NeuronOnHICANN(pyhalbe.Coordinate.X(x), y)
                self[n].enable_aout(False)
        self[neuron].enable_aout(True)

    def enable_analog_for_neuron(self, analog, neuron):
        assert isinstance(analog, pyhalbe.Coordinate.AnalogOnHICANN)
        assert isinstance(neuron, pyhalbe.Coordinate.NeuronOnHICANN)
        odd = bool(int(neuron) % 2)
        ac = self[analog]
        if neuron < 256:
            if odd:
                ac.set_membrane_top_odd(analog_channel)
            else:
                ac.set_membrane_top_even(analog_channel)
        else:
            if odd:
                ac.set_membrane_bot_odd(analog_channel)
            else:
                ac.set_membrane_bot_even(analog_channel)

    def put_neuron_to_aout(self, analog, neuron):
        self.enable_analog_for_neuron(analog, neuron)
        self.enable_neuron_aout(neuron, exclusive=True)

    def set_neuron_fg_values_from_dict(self, data):
        assert len(data) == pyhalbe.Coordinate.NeuronOnHICANN.size
        for neuron in pyhalbe.Coordinate.NeuronOnHICANN.all():
            for k, v in data[int(neuron)]:
                self.fg_control.setNeuron(neuron, k, v)

    def get_neuron_fg_values_as_dict(self):
        data = []
        params = H.neuron_parameter.names.values()
        for neuron in pyhalbe.Coordinate.NeuronOnHICANN.all():
            d ={ (p, self.fg_control.getNeuron(neuron, p)) for p in params }
            data.append(d)
        return data


class test():
    def test_aout_exclusiveness(self):
        import random
        h = pyhalbe.HICANN()

