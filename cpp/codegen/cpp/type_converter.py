from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from .type_converters import CONVERTERS


class TypeConverter(object):

    @staticmethod
    def get(abstract_type):
        return CONVERTERS[abstract_type.__class__](abstract_type)
