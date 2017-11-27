from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from ..codegen import Codegen
from ..codegen import SIGNED_SOURCE

class CppParserCodegen(Codegen):

    def __init__(self, entries):
        super(CppParserCodegen, self).__init__()

        # Keep only one example of each unique typename
        self.unique_types = {x.memory_format.typename: x.memory_format
                             for x in entries}

    def preferred_filename(self):
        return 'EntryParser.h'

    def generate(self):
        template = """
// Copyright 2004-present Facebook. All Rights Reserved.
// %%SIGNED_SOURCE%%

#pragma once

#include <cstdint>
#include <cstring>
#include <unistd.h>

#include <stdexcept>
#include <ostream>

#include <loom/entries/EntryType.h>
#include <loom/entries/Entry.h>

namespace facebook {
namespace loom {
namespace entries {

class EntryVisitor {
public:

%%ENTRY_VISITOR_METHODS%%
};

class EntryParser {
public:

%%PARSE_METHOD%%
};

} // namespace entries
} // namespace loom
} // namespace facebook
""".lstrip()

        entry_visitor = self._generate_entry_visitor_methods()
        entry_visitor = Codegen.indent(entry_visitor)
        template = template.replace('%%ENTRY_VISITOR_METHODS%%', entry_visitor)

        to_stream = self._generate_parse_method()
        to_stream = Codegen.indent(to_stream)
        template = template.replace('%%PARSE_METHOD%%', to_stream)
        template = template.replace('%%SIGNED_SOURCE%%', SIGNED_SOURCE)
        return template

    def _generate_entry_visitor_methods(self):
        template = "virtual void visit(const %%TYPE%%& entry) = 0;"
        methods = [template.replace('%%TYPE%%', x)
                   for x in self.unique_types.keys()]

        return "\n".join(methods)

    def _generate_parse_method(self):
        template = """
static void parse(const void* src, size_t size, EntryVisitor& visitor) {
  uint8_t type = entries::peek_type(src, size);
  switch (type) {
%%CASES%%
    default: throw std::invalid_argument("Unknown type in to_stream");
  }
}
""".lstrip()

        case_template = """
case %%ID%%: {
  %%TYPE%% data;
  %%TYPE%%::unpack(data, src, size);
  visitor.visit(data);
  break;
}
""".lstrip()
        cases = [ case_template
                 .replace('%%ID%%', str(x.type_id))
                 .replace('%%TYPE%%', x.typename)
                 for x in self.unique_types.values() ]
        cases = "\n".join(cases)
        cases = Codegen.indent(cases)
        cases = Codegen.indent(cases)

        template = template.replace("%%CASES%%", cases)
        return template
