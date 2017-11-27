from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import hashlib
import re

from .codegen import Language
from .cpp import entry_types as cpp_entry_types  # noqa: E402
from .cpp import entry_structs as cpp_entry_structs  # noqa: E402
from .cpp import parser as cpp_parser  # noqa: E402
from .java import entry_types as java_entry_types  # noqa: E402


class CodegenProgram(object):
    ENTRY_TYPES = 1
    ENTRY_TYPES_CPP = 2
    ENTRY_STRUCTS = 3
    ENTRY_STRUCTS_CPP = 4
    PARSER = 5

    SUPPORTED_CODEGEN = {
        Language.CPP: {
            ENTRY_TYPES: cpp_entry_types.CppEntryTypesCodegen,
            ENTRY_TYPES_CPP: cpp_entry_types.CppEntryTypesCppCodegen,
            ENTRY_STRUCTS: cpp_entry_structs.CppEntryStructsCodegen,
            ENTRY_STRUCTS_CPP: cpp_entry_structs.CppEntryStructsCppCodegen,
            PARSER: cpp_parser.CppParserCodegen,
        },
        Language.JAVA: {ENTRY_TYPES: java_entry_types.JavaEntryTypesCodegen},
    }

    # hide the pattern from tools
    SIGNED_SOURCE_PATTERN = re.compile(
        r''.join([r'@', r'generated', r' ', r'SignedSource', r'<<.*>>']))

    # hide the pattern from tools
    SIGNED_SOURCE_REPLACE_PATTERN = ''.join(
        [r'@', r'generated', r' ', r'SignedSource', r'<<{0}>>'])
    SIGNED_SOURCE_TOKEN = ''.join(['@', 'generated', ' ', '<<SignedSource::*O*zOeWoEQle#+L!plEphiEmie@IsG>>'])

    def __init__(self, lang, mode, entries):
        known_codegen = set([
            y for x in CodegenProgram.SUPPORTED_CODEGEN.values()
              for y in x.keys()])

        if mode not in known_codegen:
            raise ValueError('Unknown mode {}'.format(mode))

        if not Language.is_valid(lang):
            raise ValueError('Unknown language {}'.format(lang))

        codegen_class = CodegenProgram.SUPPORTED_CODEGEN.get(lang, {}).get(mode, None)

        if codegen_class is None:
            raise ValueError('The combination (lang: {}, mode: {}) is not '
                             'currently supported'.format(lang, mode))

        self.lang = lang
        self.mode = mode
        self.entries = entries
        self.codegen_class = codegen_class

    def run(self):
        instance = self.codegen_class(self.entries)
        output = instance.generate()
        output = CodegenProgram.sign_source(output)

        return output

    @staticmethod
    def sign_source(text):

        in_text_pattern = CodegenProgram.SIGNED_SOURCE_REPLACE_PATTERN.format('')
        fixed_token_text = text.replace(in_text_pattern, CodegenProgram.SIGNED_SOURCE_TOKEN)

        digest = hashlib.md5(fixed_token_text).hexdigest()

        actual_hash = CodegenProgram.SIGNED_SOURCE_REPLACE_PATTERN.format(digest)

        return CodegenProgram.SIGNED_SOURCE_PATTERN.sub(actual_hash, text)
