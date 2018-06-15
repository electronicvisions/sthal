import unittest
import warnings

class TestImportWarnings(unittest.TestCase):
    def setUp(self):
        import pylogging
        pylogging.default_config(pylogging.LogLevel.FATAL)

    def test_import(self):
        # Make tests fail, if there are warnings on module import
        with warnings.catch_warnings(record=True) as w:
            warnings.filterwarnings('error')
            warnings.filterwarnings('ignore', category=ImportWarning)
            import pysthal
            import pyhalbe
            import pylogging

if __name__ == '__main__':
    unittest.main()
