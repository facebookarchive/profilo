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

from ..codegen import Codegen
from ..types import ArrayType
from ..types import CompoundType
from ..types import DynamicArrayType
from ..types import EntryTypeEnum
from ..types import IntegerType
from ..types import PointerType


class CppTypeConverter(object):
    __metaclass__ = abc.ABCMeta

    def __init__(self, abstract_type):
        self.abstract_type = abstract_type

    @abc.abstractmethod
    def generate_declaration(self, name):
        pass

    @abc.abstractmethod
    def generate_pack_code(self, from_expression, to_expression, offset_expression):
        pass

    @abc.abstractmethod
    def generate_unpack_code(self, from_expression, to_expression, offset_expression):
        pass

    def generate_runtime_size_code(
        self,
        entry_expression,
        member_expression,
        offset_expression,
    ):
        return "({offset}) += sizeof({entry}.{member});".format(
            entry=entry_expression,
            member=member_expression,
            offset=offset_expression,
        )


class PrimitiveTypeConverter(CppTypeConverter):
    __metaclass__ = abc.ABCMeta

    def generate_pack_code(self, from_expression, to_expression, offset_expr):
        return """
std::memcpy(({to}) + {offset}, &({from_}), sizeof(({from_})));
{offset} += sizeof(({from_}));
""".format(
            from_=from_expression,
            to=to_expression,
            offset=offset_expr,
        )

    def generate_unpack_code(self, from_expression, to_expression, offset_expr):
        return """
std::memcpy(&({to}), ({from_}) + {offset}, sizeof(({to})));
{offset} += sizeof(({to}));
""".format(
            from_=from_expression,
            to=to_expression,
            offset=offset_expr,
        )


class IntegerTypeConverter(PrimitiveTypeConverter):
    def __init__(self, abstract_type):
        super(IntegerTypeConverter, self).__init__(abstract_type)

        if not isinstance(self.abstract_type, IntegerType):
            raise ValueError("abstract_type must be an instance of IntegerType")

    def generate_declaration(self, name):
        return "{type} {name};".format(
            type=self.map_type(),
            name=name,
        )

    def map_type(self):
        bits = self.abstract_type.constant_size * 8
        if bits < 8 or bits > 64:
            raise ArithmeticError("Supported types are between 8 and 64 bits")

        unsigned_char = "u" if not self.abstract_type.signed else ""
        template = "{unsigned}int{bits}_t"
        return template.format(
            unsigned=unsigned_char,
            bits=bits,
        )


class EntryTypeEnumConverter(IntegerTypeConverter):
    def __init__(self, abstract_type):
        super(EntryTypeEnumConverter, self).__init__(abstract_type)

        if not isinstance(self.abstract_type, EntryTypeEnum):
            raise ValueError("abstract_type must be an instance of EntryTypeEnum")

    def generate_declaration(self, name):
        return "EntryType {name};".format(
            name=name,
        )

    def generate_pack_code(self, from_expression, to_expression, offset_expr):
        return """
{int_type} {from_tmp} = static_cast<{int_type}>({from_});
std::memcpy(({to}) + {offset}, &({from_tmp}), sizeof(({from_tmp})));
{offset} += sizeof(({from_tmp}));
""".format(
            int_type=self.map_type(),
            from_tmp=from_expression.replace(".", "_") + "_tmp",
            from_=from_expression,
            to=to_expression,
            offset=offset_expr,
        )

    def generate_unpack_code(self, from_expression, to_expression, offset_expr):
        return """
{int_type} {to_tmp};
std::memcpy(&({to_tmp}), ({from_}) + {offset}, sizeof(({to_tmp})));
{offset} += sizeof(({to_tmp}));
{to} = static_cast<EntryType>({to_tmp});
""".format(
            int_type=self.map_type(),
            to_tmp=to_expression.replace(".", "_") + "_tmp",
            from_=from_expression,
            to=to_expression,
            offset=offset_expr,
        )


class ArrayTypeConverter(PrimitiveTypeConverter):
    def __init__(self, abstract_type):
        super(ArrayTypeConverter, self).__init__(abstract_type)

        if not isinstance(self.abstract_type, ArrayType):
            raise ValueError("abstract_type must be an instance of ArrayType")

        if not isinstance(self.abstract_type.member_type, IntegerType):
            raise ValueError("member_type must be an instance of IntegerType")

    def generate_declaration(self, name):
        member_type = IntegerTypeConverter(
            self.abstract_type.member_type,
        ).map_type()

        return "{type} {name}[{size}];".format(
            type=member_type,
            name=name,
            size=self.abstract_type.count,
        )


class PointerTypeConverter(CppTypeConverter):
    def __init__(self, abstract_type):
        super(PointerTypeConverter, self).__init__(abstract_type)

        if not isinstance(self.abstract_type, PointerType):
            raise ValueError("abstract_type must be an instance of PointerType")

        if not isinstance(self.abstract_type.pointed_to_type, IntegerType):
            raise ValueError("pointed_to_type must be an instance of IntegerType")

    def generate_declaration(self, name):
        member_type = IntegerTypeConverter(
            self.abstract_type.pointed_to_type,
        ).map_type()

        return "const {type}* {name};".format(
            type=member_type,
            name=name,
        )

    def generate_pack_code(self, *args):
        raise RuntimeError(
            "Cannot generate packing code for PointerType, use in DynamicArrayType"
        )

    def generate_unpack_code(self, *args):
        raise RuntimeError(
            "Cannot generate unpacking code for PointerType, use in DynamicArrayType"
        )

    def generate_runtime_size_code(self, *args):
        raise RuntimeError(
            "Cannot generate runtime size code for PointerType, use in DynamicArrayType"
        )


class CompoundTypeConverter(CppTypeConverter):
    def __init__(self, abstract_type):
        super(CompoundTypeConverter, self).__init__(abstract_type)

        if not isinstance(self.abstract_type, CompoundType):
            raise ValueError("abstract_type must be an instance of CompoundType")

    def generate_declaration(self, name):
        template = """struct {{
{fields}
}} {name};"""

        field_declarations = []
        for field_name, field_type in self.abstract_type.members.items():
            decl = CONVERTERS[field_type.__class__](field_type).generate_declaration(
                field_name
            )
            field_declarations.append(decl)

        field_declarations = "\n".join(field_declarations)
        template = template.format(
            fields=Codegen.indent(field_declarations),
            name=name,
        )

        return template

    @abc.abstractmethod
    def generate_pack_code(self, from_expression, to_expression, offset_expr):
        pass

    @abc.abstractmethod
    def generate_unpack_code(self, from_expression, to_expression, offset_expr):
        pass


class DynamicArrayTypeConverter(CompoundTypeConverter):
    def __init__(self, abstract_type):
        super(DynamicArrayTypeConverter, self).__init__(abstract_type)

    def generate_pack_code(self, from_expression, to_expression, offset_expr):
        template = """
auto _{size_field}_size = sizeof({from_}.{size_field});
std::memcpy(({to} + {offset}), &({from_}.{size_field}), (_{size_field}_size));
{offset} += _{size_field}_size;

auto _{values_field}_size = ({from_}.{size_field}) *
    sizeof(*{from_}.{values_field});
// Must align target on a 4-byte boundary. Assuming {to} is aligned.
{offset} = ({offset} + 0x03) & ~0x03;
std::memcpy(
  ({to} + {offset}),
  ({from_}.{values_field}),
  _{values_field}_size
);
{offset} += _{values_field}_size;
"""
        return template.format(
            from_=from_expression,
            to=to_expression,
            offset=offset_expr,
            values_field=DynamicArrayType.MEMBER_VALUES,
            size_field=DynamicArrayType.MEMBER_SIZE,
        )

    def generate_unpack_code(self, from_expression, to_expression, offset_expr):
        template = """
auto _{size_field}_size = sizeof({to}.{size_field});
std::memcpy(&({to}).{size_field}, ({from_} + {offset}), (_{size_field}_size));
{offset} += _{size_field}_size;

// Must align {values_field} on a 4-byte boundary. Assuming {from_} is aligned.
{offset} = ({offset} + 0x03) & ~0x03;

// Retains pointer to incoming data!
({to}).{values_field} = reinterpret_cast<decltype(({to}).{values_field})>(
  ({from_} + {offset})
);
{offset} += ({to}).{size_field} * sizeof(*({to}).{values_field});
"""

        return template.format(
            from_=from_expression,
            to=to_expression,
            offset=offset_expr,
            values_field=DynamicArrayType.MEMBER_VALUES,
            size_field=DynamicArrayType.MEMBER_SIZE,
            member_type=self.abstract_type.member_type,
        )

    def generate_runtime_size_code(
        self,
        entry_expression,
        member_expression,
        offset_expression,
    ):
        template = """
// Must align {entry}.{member} on a 4-byte boundary.
{offset} = ({offset} + 0x03) & ~0x03;

{offset} += sizeof({entry}.{member}.{size}) +
  {entry}.{member}.{size} * sizeof(*{entry}.{member}.{values});
""".strip()
        return template.format(
            entry=entry_expression,
            member=member_expression,
            offset=offset_expression,
            size=DynamicArrayType.MEMBER_SIZE,
            values=DynamicArrayType.MEMBER_VALUES,
        )


CONVERTERS = {
    ArrayType: ArrayTypeConverter,
    DynamicArrayType: DynamicArrayTypeConverter,
    IntegerType: IntegerTypeConverter,
    PointerType: PointerTypeConverter,
    EntryTypeEnum: EntryTypeEnumConverter,
}
