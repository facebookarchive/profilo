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


from .importer.interpreter import TraceFileInterpreter
from .importer.trace_file import TraceFile

if __name__ == "__main__":
    import gzip
    import sys

    with gzip.open(sys.argv[1]) as f:
        tracefile = TraceFile.from_file(f)
        interpreter = TraceFileInterpreter(tracefile)
        trace = interpreter.interpret()

        for unit in trace.executionUnits.values():
            print(repr(unit))
            for block_id in unit.blocks:
                block = trace.blocks[block_id]
                print("  {}".format(repr(block)))
                for point in block.points:
                    print("    {}".format(repr(point)))
