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


from ..model.build import Trace, StackTrace
from .trace_file import StandardEntry, BytesEntry
from .constants import COUNTER_NAMES

from functools import cmp_to_key

def entry_compare(x, y):
    """Comparison function for two StandardEntries.

    This function establishes a global order by sorting on the
    (timestamp, id) tuple.
    """
    assert x and y
    if x.timestamp == y.timestamp:
        return x.id - y.id
    else:
        return x.timestamp - y.timestamp

def block_compare(x, y):
    xs, xe = x.begin_point, x.end_point
    assert xs or xe, "No begin or end: {}".format(x)
    ys, ye = y.begin_point, y.end_point
    assert ys or ye, "No begin or end: {}".format(y)

    if xs is None and ys is not None:
        # Missing start blocks before everything else.
        # ---------------------| xe
        # ys |--------| ye
        return -1
    elif xs is not None and ys is None:
        # same as above but inverted
        return 1
    elif xs is None and ys is None:
        #  --------------| xe
        #  -----------| ye
        assert xe is not None and ye is not None

        # invert the normal sort, so xe goes first
        result = -1 * entry_compare(xe, ye)
        assert result != 0, "entries can never be equal"
        return result
    else:
        # Only remaining case - both blocks are fully formed
        assert xs is not None and ys is not None
        return entry_compare(xs, ys)

class BlockEntries(object):
    def __init__(self, begin=None, end=None):
        self.begin = begin
        self.end = end

class TraceFileInterpreter(object):

    def __init__(self, trace_file, symbols=None):
        self.trace_file = trace_file
        self.units = {}
        self.block_entries = {}
        self.symbols = symbols

    def __calculate_parents_children(self):
        """Calculate the parent-child relationships of all entries"""
        entries = {}  # id -> entry
        parents = {}  # entry -> entry
        children = {}  # entry -> [entry]

        ignore_parent_entries = {
            "CPU_COUNTER",   # arg2 == "core number"
        }

        for entry in self.trace_file.entries:
            entries[entry.id] = entry
            if isinstance(entry, StandardEntry):
                # For some entry types, arg2 is not the "parent" entry
                if entry.type in ignore_parent_entries:
                    continue
                parent_id = entry.arg2
            elif isinstance(entry, BytesEntry):
                parent_id = entry.arg1

            if parent_id != 0:
                parent = entries.get(parent_id)
                if parent:
                    parents[entry] = parent
                    children.setdefault(parent, []).append(entry)

        return parents, children

    def interpret(self):
        self.parents, self.children = self.__calculate_parents_children()

        self.trace = Trace(
            begin = min(
                (x.timestamp for x in self.trace_file.entries
                    if hasattr(x, 'timestamp'))
            ),
            end = max(
                (x.timestamp for x in self.trace_file.entries
                    if hasattr(x, 'timestamp'))
            ),
            id=self.trace_file.headers.get('id'),
        )
        thread_items = {}
        framework_frames = {}   # method_id -> full name
        for entry in self.trace_file.entries:
            if entry.type == "JAVA_FRAME_NAME":
                for child in self.children[entry]:
                    framework_frames[entry.arg3] = child.data
                continue
            if isinstance(entry, StandardEntry):
                thread_items.setdefault(entry.tid, []).append(entry)
            # BytesEntries will be processed as children of the above.

        BLOCK_START_ENTRIES = ["MARK_PUSH", "IO_START"]
        BLOCK_END_ENTRIES = ["MARK_POP", "IO_END"]

        THREAD_METADATA_ENTRIES = ["TRACE_THREAD_NAME", "TRACE_THREAD_PRI"]

        for tid, items in thread_items.items():
            entries = list(
                sorted(items, key=cmp_to_key(entry_compare))
            )
            unit = self.ensure_unit(tid)

            stacks = {}  # timestamp -> [addresses]

            # First, build blocks.
            for entry in entries:
                block = None
                if entry.type in BLOCK_START_ENTRIES:
                    block = unit.push_block(entry.timestamp)
                    self.block_entries.setdefault(
                        block, BlockEntries()).begin = entry
                elif entry.type in BLOCK_END_ENTRIES:
                    block = unit.pop_block(entry.timestamp)
                    self.block_entries.setdefault(
                        block, BlockEntries()).end = entry
                elif entry.type == "STACK_FRAME":
                    # While we're here, build the stack trace maps.
                    stacks.setdefault(entry.timestamp, []).append(entry.arg3)
                elif entry.type in THREAD_METADATA_ENTRIES:
                    self.process_thread_metadata(entry)

                if block:
                    block_entries = self.block_entries[block]
                    self.assign_name(block, entries=[
                        block_entries.begin,
                        block_entries.end,
                    ])

            unit.normalize_blocks()

            """Attach single points. We can't do this as part of the
            block-building pass due to this edge case of unbalanced pops:

            PUSH(1)  POP(2)  COUNTER(3)  POP(4)

            We want this to create the following:
            [        (3) (4)]
            [(1) (2)]

            This is impossible to achieve in a single pass because we can't
            create the block ending in (4) at the time we visit (3).
            """
            for entry in entries:
                if entry.type == "COUNTER":
                    item = unit.add_point(entry.timestamp)
                    item.properties.add_counter(
                        name=COUNTER_NAMES[entry.arg1],
                        value=entry.arg3,
                    )

                    self.assign_name(item, entries=[
                        entry
                    ])
                elif entry.type == "STACK_FRAME":
                    if entry.timestamp in stacks:
                        # we haven't written this stack trace yet, proceed
                        item = unit.add_point(entry.timestamp)
                        self.assign_name(item, entries=[
                            entry
                        ])
                        stacktrace = StackTrace()
                        for frame in stacks[entry.timestamp]:
                            symbol = None
                            if self.symbols:
                                symbol = self.symbols.method_index.get(frame, None)
                                if symbol is None:
                                    # Let's see if it's a framework frame
                                    symbol = framework_frames.get(frame, None)
                            stacktrace.append(identifier=frame, symbol=symbol)

                        item.properties.stackTraces.update({
                            'stacks': stacktrace,
                        })

                        # clear the entry in the map so we don't add a point
                        # for every frame
                        del stacks[entry.timestamp]
                        pass

        return self.trace

    def process_thread_metadata(self, entry):
        assert isinstance(entry, StandardEntry)
        if entry.type == "TRACE_THREAD_PRI":
            unit = self.ensure_unit(entry.tid)
            unit.properties.coreProps['priority'] = entry.arg3
        elif entry.type == "TRACE_THREAD_NAME":
            key_child = self.children.get(entry, [])
            assert len(key_child) == 1
            key_child = key_child[0]
            assert key_child.type == "STRING_KEY"
            tid = key_child.data

            value_child = self.children.get(key_child, [])
            assert len(value_child) == 1
            value_child = value_child[0]
            assert value_child.type == "STRING_VALUE"
            tname = value_child.data

            unit = self.ensure_unit(tid)
            currname = unit.properties.coreProps['name']
            if 'Main' in currname:
                unit.properties.coreProps['name'] = "(Main) {tname}".format(tname=tname)
            else:
                unit.properties.coreProps['name'] = "{tname}".format(tname=tname)


    def ensure_unit(self, tid):
        tid = str(tid)
        if tid == self.trace_file.headers.get('pid', None):
            name = "Main Thread_{}".format(tid)
        else:
            name = "Thread_{}".format(tid)

        unit = self.units.get(name)
        if not unit:
            unit = self.trace.add_unit()
            unit.properties.coreProps['name'] = name
            unit.properties.customProps['tid'] = tid
            unit.properties.coreProps['priority'] = 0
            self.units[name] = unit
        return unit

    def __find_name_by_string_key_value(self, entry):
        # Look for a STRING_KEY == "__name".
        # Name is in the chained STRING_VALUE.
        keys = (x for x in self.children.get(entry, [])
                if entry and x.type == 'STRING_KEY')
        keys = [x for x in keys if x.data == '__name']
        assert len(keys) <= 1

        if not keys:
            # Alternatively, look for a STRING_NAME entry.
            return None

        key = keys[0]

        # If the trace was stopped at an inopportune time, we might miss
        # the VALUE of a KEY. This is malformed beyond repair, so
        # avoid creating an entry altogether.
        if key not in self.children:
            return None

        assert len(self.children[key]) == 1
        value = self.children[key][0]

        assert value.type == 'STRING_VALUE'
        return value.data

    def __find_name_by_string_name(self, entry):
        # Look for a STRING_NAME child.
        children = [x for x in self.children.get(entry, [])
                    if entry and x.type == 'STRING_NAME']
        assert len(children) <= 1

        if not children:
            return None

        return children[0].data

    def assign_name(self, trace_element, entries=[]):
        pattern = "{}"
        name_entries = list(entries)
        if len(name_entries) == 2:
            if entries[0] and not entries[1]:
                pattern = "{} to Missing"
            elif not entries[0] and entries[1]:
                pattern = "Missing to {}"

        name = None
        for entry in name_entries:
            if not entry:
                continue

            # Try the STRING_KEY/STRING_VALUE approach first.
            name = self.__find_name_by_string_key_value(entry)

            if not name:
                # Try STRING_NAME instead.
                name = self.__find_name_by_string_name(entry)

            if name:
                break

        if not name:
            name = [x.type for x in name_entries if x]
            name = " to ".join(name)

        trace_element.properties.coreProps['name'] = pattern.format(name)
