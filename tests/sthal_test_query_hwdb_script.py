#!/usr/bin/env python

import unittest
import os
import subprocess

import pysthal


class TestSthalSettings(unittest.TestCase):
    def get_command_stdout(self, command, params):
        base_command = [self.scriptname, "--hwdb", self.dbpath]

        process = subprocess.Popen(
            base_command + [command] + params,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

        stdout, _ = process.communicate()

        return stdout

    def get_return_code(self, command, params):
        base_command = [self.scriptname, "--hwdb", self.dbpath]

        process = subprocess.Popen(
            base_command + [command] + params,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

        process.wait()

        return process.returncode

    def setUp(self):
        thispath = os.path.abspath(
            os.path.dirname(__file__))
        self.dbpath = os.path.join(
            pysthal.Settings.get().datadir,
            'testdata',
            'sthal_test_query_hwdb_script.yaml')
        self.scriptname = "query_hwdb"

    def test_test_setup(self):
        self.assertTrue(os.path.exists(self.dbpath))

    def test_hicann_version(self):
        stdout = self.get_command_stdout("hicann-version", ['--wafer', '1340', '--hicann', '88'])
        self.assertEqual(stdout, '2\n')

        stdout = self.get_command_stdout("hicann-version", ['--wafer', '1340', '--hicann', '89'])
        self.assertEqual(stdout, '4\n')

        stdout = self.get_command_stdout("hicann-version", ['--wafer', '1339', '--hicann', '89'])
        self.assertEqual(stdout, '2\n')

    def test_hicann_label(self):
        stdout = self.get_command_stdout("hicann-label", ['--wafer', '1340', '--hicann', '88'])
        self.assertEqual(stdout, '\n')

        stdout = self.get_command_stdout("hicann-label", ['--wafer', '1340', '--hicann', '89'])
        self.assertEqual(stdout, 'v4-5\n')

        stdout = self.get_command_stdout("hicann-label", ['--wafer', '1340', '--hicann', '29,4'])
        self.assertEqual(stdout, 'v4-5\n')

        stdout = self.get_command_stdout("hicann-label", ['--wafer', '1340', '--hicann', '90'])
        self.assertEqual(stdout, 'somehicannv2label\n')

    def test_ipc(self):
        stdout = self.get_command_stdout("ipc-filename", ['--wafer', '1340', '--hicann', '90'])
        self.assertEqual(stdout, '/dev/shm/192.168.4.4\n')

        stdout = self.get_command_stdout("ipc-filename", ['--wafer', '1340', '--hicann', '88'])
        self.assertEqual(stdout, '/dev/shm/192.168.4.4\n')

        stdout = self.get_command_stdout("ipc-filename", ['--wafer', '1340', '--hicann', '144'])
        self.assertEqual(stdout, '/dev/shm/192.168.4.1\n')

        stdout = self.get_command_stdout("ipc-filename", ['--wafer', '1340', '--hicann', '180'])
        self.assertEqual(stdout, '/dev/shm/192.168.4.1\n')

        stdout = self.get_command_stdout("ipc-filename", ['--wafer', '1339', '--hicann', '257'])
        self.assertEqual(stdout, '/dev/shm/192.168.21.106\n')


    def test_devtype(self):
        stdout = self.get_command_stdout("device-type", ['--wafer', '1337'])
        self.assertEqual(stdout, 'FACETSWafer\n')

        stdout = self.get_command_stdout("device-type", ['--wafer', '1338'])
        self.assertEqual(stdout, 'CubeSetup\n')

        stdout = self.get_command_stdout("device-type", ['--wafer', '1339'])
        self.assertEqual(stdout, 'BSSWafer\n')

        stdout = self.get_command_stdout("device-type", ['--wafer', '1340'])
        self.assertEqual(stdout, 'CubeSetup\n')

    def test_inexistent(self):
        retcode = self.get_return_code("device-type", ['--wafer', '21259'])
        self.assertNotEqual(retcode, os.EX_OK)

        retcode = self.get_return_code("device-type", ['--wafer', '1337'])
        self.assertEqual(retcode, os.EX_OK)

        retcode = self.get_return_code("hicann-version", ['--wafer', '1337', '--hicann', '91'])
        self.assertNotEqual(retcode, os.EX_OK)

        retcode = self.get_return_code("hicann-version", ['--wafer', '1339', '--hicann', '91'])
        self.assertEqual(retcode, os.EX_OK)

        retcode = self.get_return_code("hicann-version", ['--wafer', '1340', '--hicann', '91'])
        self.assertNotEqual(retcode, os.EX_OK)

        retcode = self.get_return_code("hicann-version", ['--wafer', '1340', '--hicann', '88'])
        self.assertEqual(retcode, os.EX_OK)

    def test_macu(self):
        stdout = self.get_command_stdout("macu", ['--wafer', '1337'])
        self.assertEqual(stdout, '0.0.0.0\n')

        stdout = self.get_command_stdout("macu", ['--wafer', '1340'])
        self.assertEqual(stdout, '1.2.3.4\n')

if __name__ == '__main__':
    unittest.main()
