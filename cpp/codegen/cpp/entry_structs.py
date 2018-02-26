from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from ..codegen import Codegen
from ..codegen import SIGNED_SOURCE

from ..types import DynamicArrayType
from .type_converter import TypeConverter

class CppEntryStructsCodegen(Codegen):

    def __init__(self, entries):
        super(CppEntryStructsCodegen, self).__init__()

        # Keep only one example of each unique typename
        self.unique_types = {x.memory_format.typename: x.memory_format for x in entries}

    def preferred_filename(self):
        return 'Entry.h'

    def generate(self):
        template = """
// Copyright 2004-present Facebook. All Rights Reserved.
// %%SIGNED_SOURCE%%

#include <cstdint>
#include <cstring>
#include <type_traits>
#include <unistd.h>

#pragma once

namespace facebook {
namespace profilo {
namespace entries {

%%ENTRIES_STRUCTS%%

uint8_t peek_type(const void* src, size_t len);

} // namespace entries
} // namespace profilo
} // namespace facebook
""".lstrip()

        enum = self._generate_entries_structs()
        template = template.replace('%%ENTRIES_STRUCTS%%', enum)
        template = template.replace('%%SIGNED_SOURCE%%', SIGNED_SOURCE)
        return template

    def _generate_entries_structs(self):

        structs = [self._generate_entry_struct(fmt) for fmt in
                   self.unique_types.values()]

        structs = "\n".join(structs)

        return structs

    def _generate_entry_struct(self, fmt):
        template = """
struct __attribute__((packed)) %%TYPENAME%% {

  static const uint8_t kSerializationType = %%TYPE_ID%%;

%%FIELDS%%

  static void pack(const %%TYPENAME%%& entry, void* dst, size_t size);
  static void unpack(%%TYPENAME%%& entry, const void* src, size_t size);

  static size_t calculateSize(%%TYPENAME%% const& entry);
};
""".lstrip()

        fields = [
            TypeConverter.get(field[1]).generate_declaration(name=field[0])
            for field in fmt.fields
        ]
        fields = "\n".join(fields)
        fields = Codegen.indent(fields)

        template = template.replace('%%TYPENAME%%', fmt.typename)
        template = template.replace('%%TYPE_ID%%', str(fmt.type_id))
        template = template.replace('%%FIELDS%%', fields)

        return template

        template = template.replace('%%TYPENAME%%', fmt.typename)

class CppEntryStructsCppCodegen(Codegen):

    def __init__(self, entries):
        super(CppEntryStructsCppCodegen, self).__init__()

        # Keep only one example of each unique typename
        self.unique_types = {x.memory_format.typename: x.memory_format for x in entries}

    def preferred_filename(self):
        return 'Entry.cpp'

    def generate(self):
        template = """
// Copyright 2004-present Facebook. All Rights Reserved.
// %%SIGNED_SOURCE%%

#include <cstring>
#include <stdexcept>
#include <loom/entries/Entry.h>

namespace facebook {
namespace profilo {
namespace entries {

%%ENTRIES_CODE%%

uint8_t peek_type(const void* src, size_t len) {
  const uint8_t* src_byte = reinterpret_cast<const uint8_t*>(src);
  return *src_byte;
}

} // namespace entries
} // namespace profilo
} // namespace facebook
""".lstrip()

        code = self._generate_entries_code()
        template = template.replace('%%ENTRIES_CODE%%', code)
        template = template.replace('%%SIGNED_SOURCE%%', SIGNED_SOURCE)
        return template

    def _generate_entries_code(self):

        structs = [self._generate_entry_struct(fmt) for fmt in
                   self.unique_types.values()]

        structs = "\n".join(structs)

        return structs

    def _generate_entry_struct(self, fmt):
        template = """
%%PACKCODE%%

%%UNPACKCODE%%

%%CALCULATESIZECODE%%
""".lstrip()

        pack_code = self._generate_pack_code(fmt)
        unpack_code = self._generate_unpack_code(fmt)
        calcsize_code = self._generate_calcsize_code(fmt)

        template = template.replace('%%PACKCODE%%', pack_code)
        template = template.replace('%%UNPACKCODE%%', unpack_code)
        template = template.replace('%%CALCULATESIZECODE%%', calcsize_code)

        return template

    def _generate_pack_code(self, fmt):
        template = """
/* Alignment requirement: dst must be 4-byte aligned. */
void %%TYPENAME%%::pack(const %%TYPENAME%%& entry, void* dst, size_t size) {
  if (size < %%TYPENAME%%::calculateSize(entry)) {
      throw std::out_of_range("Cannot fit %%TYPENAME%% in destination");
  }
  if (dst == nullptr) {
      throw std::invalid_argument("dst == nullptr");
  }
  uint8_t* dst_byte = reinterpret_cast<uint8_t*>(dst);
  *dst_byte = kSerializationType;
  size_t offset = 1;

%%MEMCOPIES%%
}
""".lstrip()

        memcopies = []
        for idx, (name, ftype) in enumerate(fmt.fields):

            if isinstance(ftype, DynamicArrayType) and idx != len(fmt.fields) - 1:
                # HACK: figure out how to propagate dynamic offsets in the
                # packing/unpacking code
                raise ValueError("DynamicArrayType entries are only allowed"
                    " as the last member")

            memcpy = TypeConverter.get(ftype).generate_pack_code(
                from_expression="entry.{name}".format(name=name),
                to_expression="dst_byte",
                offset_expr="offset",
            )
            memcopies.append(memcpy)
        memcopies = "\n".join(memcopies)
        memcopies = Codegen.indent(memcopies)

        template = template.replace('%%TYPENAME%%', fmt.typename)
        template = template.replace('%%MEMCOPIES%%', memcopies)
        return template

    def _generate_unpack_code(self, fmt):
        template = """
/* Alignment requirement: src must be 4-byte aligned. */
void %%TYPENAME%%::unpack(%%TYPENAME%%& entry, const void* src, size_t size) {
  if (src == nullptr) {
      throw std::invalid_argument("src == nullptr");
  }
  const uint8_t* src_byte = reinterpret_cast<const uint8_t*>(src);
  if (*src_byte != kSerializationType) {
      throw std::invalid_argument("Serialization type is incorrect");
  }
  size_t offset = 1;
%%MEMCOPIES%%
}
""".lstrip()

        memcopies = []
        for name, ftype in fmt.fields:
            memcpy = TypeConverter.get(ftype).generate_unpack_code(
                from_expression="src_byte",
                to_expression="entry.{name}".format(name=name),
                offset_expr="offset",
            )
            memcopies.append(memcpy)
        memcopies = "\n".join(memcopies)

        memcopies = Codegen.indent(memcopies)

        template = template.replace('%%TYPENAME%%', fmt.typename)
        template = template.replace('%%MEMCOPIES%%', memcopies)
        return template

    def _generate_calcsize_code(self, fmt):
        template = """
size_t %%TYPENAME%%::calculateSize(%%TYPENAME%% const& entry) {
  size_t offset = 1 /*serialization format*/;
%%EXPRESSIONS%%
  return offset;
}
""".lstrip()

        expressions = [
            TypeConverter.get(ftype).generate_runtime_size_code(
                "entry",
                fname,
                "offset",
            ) for fname, ftype in fmt.fields
        ]
        expressions = "\n".join(expressions)
        expressions = Codegen.indent(expressions)

        template = template.replace('%%TYPENAME%%', fmt.typename)
        template = template.replace('%%EXPRESSIONS%%', expressions)
        return template
