"""
Copyright 2018-present, Facebook, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import abc


class Type(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractproperty
    def constant_size(self):
        pass


class PrimitiveType(Type):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def __init__(self, size):
        super(PrimitiveType, self).__init__()
        self.size = size

    @property
    def constant_size(self):
        return self.size


class IntegerType(PrimitiveType):
    def __init__(self, size, signed):
        super(IntegerType, self).__init__(size)
        self.signed = signed


class EntryTypeEnum(IntegerType):
    "Represents the code-generated EntryType enum"

    def __init__(self):
        super(EntryTypeEnum, self).__init__(size=1, signed=False)


class ArrayType(PrimitiveType):
    def __init__(self, member_type, count):
        super(ArrayType, self).__init__(size=member_type.constant_size * count)
        self.member_type = member_type
        self.count = count


class PointerType(PrimitiveType):

    PTR_SIZE = 4  # TODO: support 64-bit pointers

    def __init__(self, pointed_to_type):
        super(PointerType, self).__init__(size=PointerType.PTR_SIZE)
        self.pointed_to_type = pointed_to_type


class CompoundType(Type):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def __init__(self, members):
        super(CompoundType, self).__init__()

        self.members = members

    @property
    def constant_size(self):
        return sum([member.constant_size for member in self.members.values()])


class DynamicArrayType(CompoundType):

    MEMBER_VALUES = "values"
    MEMBER_SIZE = "size"

    def __init__(self, member_type):
        super(DynamicArrayType, self).__init__(
            members={
                DynamicArrayType.MEMBER_VALUES: PointerType(member_type),
                DynamicArrayType.MEMBER_SIZE: Types.uint16,
            }
        )
        self.member_type = member_type


class Types(object):
    __metaclass__ = abc.ABCMeta

    int8 = IntegerType(size=1, signed=True)
    int16 = IntegerType(size=2, signed=True)
    int32 = IntegerType(size=4, signed=True)
    int64 = IntegerType(size=8, signed=True)
    uint8 = IntegerType(size=1, signed=False)
    uint16 = IntegerType(size=2, signed=False)
    uint32 = IntegerType(size=4, signed=False)
    uint64 = IntegerType(size=8, signed=False)
