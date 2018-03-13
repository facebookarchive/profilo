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

from __future__ import division
from __future__ import unicode_literals
from __future__ import absolute_import
from __future__ import print_function

import sys
import argparse
import importlib

from .codegen import Language
from .codegen_program import CodegenProgram
from . import configloader

if __name__ == '__main__':

    MODES = {
        'entry_types': CodegenProgram.ENTRY_TYPES,
        'entry_types_cpp': CodegenProgram.ENTRY_TYPES_CPP,
        'entry_structs': CodegenProgram.ENTRY_STRUCTS,
        'entry_structs_cpp': CodegenProgram.ENTRY_STRUCTS_CPP,
        'parser': CodegenProgram.PARSER,
    }

    parser = argparse.ArgumentParser(description='Codegen log entry code.')
    parser.add_argument(
        '--lang',
        dest='lang',
        action='store',
        required=True,
        help='Language to generate the output in. Available options: '
        'java and cpp.')

    parser.add_argument(
        '--mode',
        dest='mode',
        action='store',
        required=True,
        default='entry_types',
        help='Type of codegen. Available options: %s' % (list(MODES.keys()), ))

    parser.add_argument(
        '--module',
        dest='module',
        action='store',
        required=True,
        help='Config module to generate code for.')

    parser.add_argument(
        '--debug',
        dest='debug',
        action='store_true',
        help='Drop into an interactive shell.')
    args = parser.parse_args()

    lang = None
    if args.lang.lower() == 'java':
        lang = Language.JAVA
    elif args.lang.lower() == 'cpp':
        lang = Language.CPP

    if args.mode.lower() not in MODES:
        raise ValueError('Unrecognized mode %s. Known modes: %s' % (
            args.mode, list(MODES.keys())))

    mode = MODES[args.mode.lower()]

    if lang is None:
        raise ValueError('Language must be "java" or "cpp"')

    config_module = configloader.load_module(args.module)
    entries = config_module.get_entry_descriptions()

    program = CodegenProgram(lang=lang, mode=mode, entries=entries)

    if args.debug:
        import code
        code.interact(
            banner="Dropping into interactive shell.",
            local=dict(globals(), **locals()),
        )

    sys.stdout.write(program.run())
