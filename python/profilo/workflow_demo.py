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


from .importer.trace_file import TraceFile
from .importer.interpreter import TraceFileInterpreter
from .model import ttypes as tt
from .symbols.apk_symbols import extract_apk_symbols

from collections import defaultdict
from collections import namedtuple

import numpy as np
import argparse
import pandas as pd
import zipfile


def blocks(tracefile, args):
    """
    Get per-thread statistics of writes to atrace
    (http://androidxref.com/7.0.0_r1/xref/system/core/include/utils/Trace.h#45)
    in the form "is_main,thread_id,event,mean,median,std".

    Times are in nanoseconds.

    This script prints the output as a list of csv's in stdout.
    """
    interpreter = TraceFileInterpreter(tracefile)
    trace = interpreter.interpret()
    thread_blocks = {}  # tid -> [(block_name, duration)]

    main_thread = -1

    for unit in trace.executionUnits.values():
        tid = int(unit.properties.customProps['tid'])
        name = unit.properties.coreProps['name']
        if 'Main' in name:
            main_thread = tid
        thread_blocks[tid] = []
        for block_id in unit.blocks:
            block = trace.blocks[block_id]
            name = block.properties.coreProps.get('name')
            if not name:
                continue
            time_start = block.begin_point.timestamp
            time_end = block.end_point.timestamp
            thread_blocks[tid].append([name, time_end - time_start])

    # Group block durations by function name
    thread_functions = {}   # tid -> block_name -> [counts]
    for tid, blocks in thread_blocks.items():
        thread_functions[tid] = defaultdict(lambda: [])
        for block in blocks:
            thread_functions[tid][block[0]].append(block[1])

    Statistics = namedtuple('Statistics', 'mean median std')

    stats = defaultdict(lambda: {})  # tid -> block_name -> Statistics

    for tid, functions in thread_functions.items():
        for func, counts in functions.items():
            mean = np.mean(counts)
            median = np.median(counts)
            std = np.std(counts)
            stats[tid][func] = Statistics(mean=mean, median=median, std=std)

    # DataFrame dictionary
    df_dict = {'IsMain': [], 'tid': [], 'event': [], 'mean': [], 'median': [],
               'std': []}

    for tid, functions in stats.items():
        for func, stat in functions.items():
            df_dict['IsMain'].append('True' if tid == main_thread else 'False')
            df_dict['tid'].append(tid)
            df_dict['event'].append(func)
            df_dict['mean'].append(stat.mean)
            df_dict['median'].append(stat.median)
            df_dict['std'].append(stat.std)

    dataframe = pd.DataFrame(data=df_dict, columns=['IsMain', 'tid',
                             'event', 'mean', 'median', 'std'])

    print(dataframe.to_csv())


def rescale(lst):
    """
    Rescale the timestamps to the range [0,100].

    lst = [(timestamp, count)]
    """

    ts, counts = list(zip(*lst))
    ts = np.array(ts)
    rescaled = 100 * ((ts - ts.min()) / (1 + (float(ts.max()) - float(ts.min()))))
    return list(zip(rescaled, counts))


def syscounters(tracefile, args):
    """
    Parse system counter information from a trace file, sort by increasing
    timestamp order, and plot as a time series.
    """
    plotdir = os.path.join(args.plotdir, '')
    # Validate output directory actually exists
    if not os.path.exists(args.plotdir):
        print("syscounters: {plt}: no such file or directory".format(plt=args.plotdir))
        sys.exit(2)

    interpreter = TraceFileInterpreter(tracefile)
    trace = interpreter.interpret()

    counters = defaultdict(lambda: [])  # counter_type -> [(timestamp, counter)]

    for unit in trace.executionUnits.values():
        for block_id in unit.blocks:
            block = trace.blocks[block_id]
            for point in block.points:
                counter = point.properties.counterProps.get(tt.CounterUnit.ITEMS, None)
                if not counter:
                    continue
                for counterType, count in counter.items():
                    counters[counterType].append((point.timestamp, count))

    for counterType, series in counters.items():
        counters[counterType] = rescale(counters[counterType])
        counters[counterType].sort(key=lambda x: x[0])

    # Now <counters> is a time series for each counter type. We can plot a few
    # of them
    import matplotlib.pyplot as plt
    test_counters = ['NUM_PROCS', 'PROC_CPU_TIME', 'GLOBAL_ALLOC_SIZE',
                     'ALLOC_FREE_BYTES']

    for tc in test_counters:
        data = counters[tc]
        x, y = list(zip(*data))
        plt.xlabel('time')
        plt.ylabel(tc.lower())
        plt.plot(x, y)
        plt.axis([min(x), max(x), min(y), max(y)])
        plt.savefig(plotdir + tc.lower())
        plt.clf()


def stacks(tracefile, args):
    """
    Simple analyzer for stack traces. Outputs stacks in a format similar
    to that of DTrace. Further analysis of the data can be done by tools
    such as FlameGraph
    (https://github.com/brendangregg/FlameGraph)

    This script prints its output to stdout.
    """

    # Validate that the supplied apk exists
    if not os.path.exists(args.apk):
        print("stacks: {apk}: no such file or directory".format(apk=args.apk))
        sys.exit(2)

    symbols = extract_apk_symbols(args.apk)
    interpreter = TraceFileInterpreter(tracefile, symbols)
    trace = interpreter.interpret()
    stacks = defaultdict(int)   # [frame] -> int (count)

    for unit in trace.executionUnits.values():
        for block_id in unit.blocks:
            block = trace.blocks[block_id]
            for point in block.points:
                stackObject = point.properties.stackTraces.get('stacks', None)
                if not stackObject:
                    continue
                stackFrames = stackObject.frames
                singleStack = []
                for frame in stackFrames:
                    if frame.symbol:
                        # We actually know the symbol for this address.
                        singleStack.insert(0, frame.symbol)
                    else:
                        # No symbolication luck here :(
                        singleStack.insert(0, frame.identifier)
                stacks[tuple(singleStack)] += 1

    # DTrace collapser ignores the first three lines
    print("\n\n\n")
    for k, v in stacks.items():
        for frame in k:
            if str(frame).isdigit():
                print("_", frame, sep='')
            else:
                print(frame, sep='')
        print(v)
        print("\n")


if __name__ == "__main__":
    """
    Driver for demo workflows. Can run the following analysis:

        -> Stacks
        -> Blocks
        -> System counters
    """
    import sys
    import gzip
    import os

    # Top level parser. The trace file is common for all types of demos,
    # so parse it here.
    parser = argparse.ArgumentParser(description="Profilo workflow demo")
    parser.add_argument('trace', type=str, help="Path to downloaded trace")

    subparsers = parser.add_subparsers(help="Demo types")

    # Parser for stack-specific arguments
    stack_parser = subparsers.add_parser('stacks', help="Analyze stacks")
    stack_parser.add_argument('apk', type=str,
                              help="Path to apk (for symbolication")
    stack_parser.set_defaults(func=stacks)

    # Parser for block-specific arguments
    block_parser = subparsers.add_parser('blocks', help="Analyze blocks")
    block_parser.set_defaults(func=blocks)

    # Parser for syscounter-specific arguments
    syscounter_parser = subparsers.add_parser('syscounters',
                                              help="Analyze system counters")
    syscounter_parser.add_argument('plotdir', type=str,
                                   help="Path where plots will be generated")
    syscounter_parser.set_defaults(func=syscounters)

    args = parser.parse_args()

    # Make sure the trace file exists
    if not os.path.exists(args.trace):
        print(args.trace, "no such file or directory")
        sys.exit(2)

    # Unzip first if the file is zipped
    if zipfile.is_zipfile(args.trace):
        main_found = False
        with zipfile.ZipFile(args.trace) as zipped:
            for elem in zipped.namelist():
                if elem.startswith("main-"):
                    main_found = True
                    with gzip.open(zipped.open(elem)) as fd:
                        tracefile = TraceFile.from_file(fd)
                    break
            if not main_found:
                print("Did not find trace inside zip file")
                sys.exit(3)
    else:
        with gzip.open(args.trace) as fd:
            tracefile = TraceFile.from_file(fd)

    args.func(tracefile, args)
