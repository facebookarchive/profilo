from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from ..codegen import Codegen
from ..codegen import SIGNED_SOURCE


class CppEntryTypesCodegen(Codegen):

    def __init__(self, entries):
        super(CppEntryTypesCodegen, self).__init__()
        self.entries = entries

    def preferred_filename(self):
        return 'EntryType.h'

    def generate(self):
        template = """
// Copyright 2004-present Facebook. All Rights Reserved.
// %%SIGNED_SOURCE%%

#pragma once

namespace facebook {
namespace loom {
namespace entries {
%%ENTRIES_ENUM%%

const char* to_string(EntryType type);
} // namespace entries
} // namespace loom
} // namespace facebook
""".lstrip()

        template = template.replace('%%ENTRIES_ENUM%%', self._generate_entries_enum())
        template = template.replace('%%SIGNED_SOURCE%%', SIGNED_SOURCE)

        return template

    def _generate_entries_enum(self):
        template = """
enum EntryType {
%%NAME_TO_ID_ENTRIES%%
};
""".lstrip()

        name_id_entries = ['{0.name} = {0.id},'.format(x) for x in self.entries]
        name_id_entries = '\n'.join(name_id_entries)

        name_id_entries = Codegen.indent(name_id_entries)

        template = template.replace('%%NAME_TO_ID_ENTRIES%%', name_id_entries)
        return template

class CppEntryTypesCppCodegen(Codegen):

    def __init__(self, entries):
        super(CppEntryTypesCppCodegen, self).__init__()
        self.entries = entries

    def preferred_filename(self):
        return 'EntryType.cpp'

    def generate(self):
        template = """
// Copyright 2004-present Facebook. All Rights Reserved.
// %%SIGNED_SOURCE%%

#include <stdexcept>
#include <loom/entries/EntryType.h>

namespace facebook {
namespace loom {
namespace entries {

%%TO_STRING%%
} // namespace entries
} // namespace loom
} // namespace facebook
""".lstrip()

        template = template.replace('%%TO_STRING%%', self._generate_to_string())
        template = template.replace('%%SIGNED_SOURCE%%', SIGNED_SOURCE)
        return template

    def _generate_to_string(self):
        template = """
const char* to_string(EntryType type) {
  switch(type) {
%%CASES%%
    default: throw std::invalid_argument("Unknown entry type");
  }
}
""".lstrip()

        cases = ['case {0.name}: return "{0.name}";'.format(x) for x in self.entries]
        cases = '\n'.join(cases)

        cases = Codegen.indent(cases)
        cases = Codegen.indent(cases)

        template = template.replace('%%CASES%%', cases)
        return template
