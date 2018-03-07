

from .importer.trace_file import TraceFile
from .importer.interpreter import TraceFileInterpreter

import os.path
import gzip

def open_trace(filepath):
    filepath = os.path.expanduser(filepath)
    if filepath.endswith('gz'):
        fd = gzip.open(filepath, mode='rb')
    else:
        fd = open(filepath, mode='rb')

    interpreter = TraceFileInterpreter(TraceFile.from_file(fd))
    return interpreter.interpret()
