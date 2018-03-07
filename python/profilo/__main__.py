
from .importer.trace_file import TraceFile
from .importer.interpreter import TraceFileInterpreter

if __name__ == "__main__":
    import sys
    import gzip
    with gzip.open(sys.argv[1]) as f:
        tracefile = TraceFile.from_file(f)
        interpreter = TraceFileInterpreter(tracefile)
        trace = interpreter.interpret()

        for unit in trace.executionUnits.values():
            print (repr(unit))
            for block_id in unit.blocks:
                block = trace.blocks[block_id]
                print ("  {}".format(repr(block)))
                for point in block.points:
                    print ("    {}".format(repr(point)))
