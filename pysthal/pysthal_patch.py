"""
pysthal.so will load this module during initialization
"""

import _pysthal
import pyhwdb

def patch(module):
    """
    This hook will be executed at the end of the pysthal module generation
    """
    _pysthal.ADCConfig.CalibrationMode = pyhwdb.ADCEntry.CalibrationMode
