from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import importlib

def load_module(configname):
    """
    This needs to be in its own file, so __name__ refers to the imported path
    and not just the value '__main__'.
    """

    base_name = __name__.split('.')[:-1]
    base_name = '.'.join(base_name)
    return importlib.import_module('.config.' + configname, base_name)
