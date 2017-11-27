from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
import abc
import re

from collections import namedtuple

SIGNED_SOURCE = ''.join(['@', 'generated', ' ', 'SignedSource', '<<>>'])

class Language(object):
    CPP = 1
    JAVA = 2

    __metaclass__ = abc.ABCMeta

    @staticmethod
    def is_valid(num):
        return num in [Language.CPP, Language.JAVA]

class MemoryDescription(namedtuple('MemoryDescription',
                                     ['fields', 'typename'])):
    TYPE_ID = 1

    def __init__(self, **kwargs):
        super(MemoryDescription, self).__init__(self, **kwargs)

        if self.fields is None:
            raise ValueError('fields is required')

        if self.typename is None:
            raise ValueError('typename is required')

        self.type_id = MemoryDescription.TYPE_ID
        MemoryDescription.TYPE_ID += 1

class EntryDescription(
    namedtuple('EntryDescription',
               ['id', 'name', 'memory_format'])):

    def __init__(self, **kwargs):
        super(EntryDescription, self).__init__(self, **kwargs)

        if self.id is None:
            raise ValueError('id is required')

        if self.name is None:
            raise ValueError('name is required')

        if self.memory_format is None:
            raise ValueError('memory_format is required')

        if not isinstance(self.memory_format, MemoryDescription):
            raise ValueError('memory_format must be a MemoryDescription')

class Codegen(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def generate(self):
        pass

    @abc.abstractmethod
    def preferred_filename(self):
        pass

    @staticmethod
    def indent(text):
        return re.sub('^', ' ' * 2, text, flags=re.MULTILINE)
