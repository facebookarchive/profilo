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


from . import ttypes as tt

from .intervals import IntervalTree

import random
import string
BASE64_ALPHABET = (
    string.ascii_uppercase +
    string.ascii_lowercase +
    string.digits + '+/'
)

def new_id():
    num = random.randrange(1, 0xFFFFFFFFFFFFFFFF)

    data = ['A'] * 11  # 11 chars in a 64-bit num, 'A' is 0
    idx = len(data) - 1  # last char
    while num:
        denom = num % 64
        num = num // 64
        data[idx] = BASE64_ALPHABET[denom]
        idx -= 1
    return "".join(data)

class StackFrame(tt.StackFrame):
    def __init__(self, **kwargs):
        super(StackFrame, self).__init__(**kwargs)

class StackTrace(tt.StackTrace):
    def __init__(self):
        super(StackTrace, self).__init__(frames=[])

    def append(self, identifier, symbol=None, code_location=None, line_number=None):
        self.frames.append(StackFrame(
            identifier=identifier,
            symbol=symbol,
            codeLocation=code_location,
            lineNumber=line_number,
        ))


class Properties(tt.Properties):
    def __init__(self, coreProps=None, customProps=None, counterProps=None, errors=None, sets=None, stacktraces=None, stacktraceHashes=None, stacktraceHashResolvers=None,):
        super(Properties, self).__init__(coreProps, customProps, counterProps, errors, sets, stacktraces, stacktraceHashes, stacktraceHashResolvers)
        self.coreProps = coreProps if coreProps else {}
        self.customProps = customProps if customProps else {}
        self.counterProps = counterProps if counterProps else {}
        self.errors = errors if errors else []
        self.sets = sets if sets else {}
        self.stackTraces = stacktraces if stacktraces else {}

    def add_counter(self, name, value, counter_unit=tt.CounterUnit.ITEMS):
        self.counterProps.setdefault(counter_unit, {})[name] = value

class Edge(tt.Edge):
    def __init__(self, trace, sourcePoint=None, targetPoint=None):
        super(Edge, self).__init__(
            sourcePoint=sourcePoint,
            targetPoint=targetPoint,
            properties=Properties(),
        )

class Point(tt.Point):
    def __init__(self, trace, unit, block, timestamp=0, sequenceNumber=0):
        super(Point, self).__init__(
            new_id(),
            timestamp,
            unalignedTimestamp=timestamp,
            properties=Properties(),
            sequenceNumber=sequenceNumber,
        )
        self.trace = trace
        self.unit = unit
        self.block = block

    def __repr__(self):
        return ("<Point ({id}): {timestamp} \"{name}\">".format(
            id=self.id,
            timestamp=self.timestamp,
            name=self.properties.coreProps.get('name', "<None>"),
        ))

class Block(tt.Block):
    def __init__(self, trace, unit, begin=None, end=None):
        super(Block, self).__init__(new_id(), begin, end, otherPoints=[], properties=Properties())
        self.trace = trace
        self.unit = unit
        self.begin_point = None
        self.end_point = None
        self.parent = None

    def create_begin_point(self, timestamp):
        assert not self.begin

        point = Point(self.trace, self.unit, self, timestamp=timestamp)
        self.trace.points[point.id] = point
        self.begin = point.id
        self.begin_point = point
        return point

    def create_end_point(self, timestamp):
        assert not self.end

        point = Point(self.trace, self.unit, self, timestamp=timestamp)
        self.trace.points[point.id] = point
        self.end = point.id
        self.end_point = point
        return point

    def add_point(self, timestamp):
        point = Point(self.trace, self.unit, self, timestamp=timestamp)
        self.trace.points[point.id] = point
        self.otherPoints.append(point.id)
        return point

    def add_child_block(self, child):
        assert not child.parent
        call_time = child.begin_point.timestamp
        return_time = child.end_point.timestamp
        assert (self.begin_point.timestamp
            <= call_time <= return_time <= self.end_point.timestamp)

        call_from = self.add_point(call_time)
        call_to = child.add_point(call_time)
        return_from = child.add_point(return_time)
        return_to = self.add_point(return_time)

        call_edge = self.trace.add_edge(call_from, call_to)
        call_edge.properties.coreProps.update(
            {
                "edge_event_source": "call_to_block",
                "type": "nested_call",
            }
        )

        return_edge = self.trace.add_edge(return_from, return_to)
        return_edge.properties.coreProps.update(
            {
                "edge_event_source": "wait_for_block",
                "type": "nested_return",
            }
        )

        child.parent = self

    @property
    def points(self):
        all_point_ids = [self.begin] + self.otherPoints + [self.end]
        return [
            self.trace.points[pt_id] for pt_id in all_point_ids
                if pt_id is not None
        ]

    def __repr__(self):
        return ("<Block ({id}): {start} to {end} \"{name}\"; {points} points>".format(
            id=self.id,
            start=self.begin_point.timestamp if self.begin_point else 'Missing',
            end=self.end_point.timestamp if self.end_point else 'Missing',
            name=self.properties.coreProps.get('name', "<None>"),
            points=len(self.points),
        ))

class ExecutionUnit(tt.ExecutionUnit):
    def __init__(self, trace):
        super(ExecutionUnit, self).__init__(new_id(), [], Properties())
        self.trace = trace
        self.tree = None
        self.stack = []

    def add_block(self, beginTimestamp=None, endTimestamp=None):
        block = Block(self.trace, self)
        self.trace.blocks[block.id] = block
        self.blocks.append(block.id)
        if beginTimestamp:
            block.create_begin_point(beginTimestamp)
        if endTimestamp:
            block.create_end_point(endTimestamp)

        return block

    def push_block(self, timestamp):
        block = self.add_block(beginTimestamp=timestamp)
        self.stack.append(block)
        return block

    def pop_block(self, timestamp):
        if len(self.stack) == 0 or self.stack[-1].end != None:
            # Unbalanced pop - need to create a
            # block on top of the existing stack.
            block = self.add_block(endTimestamp=timestamp)
            self.stack.append(block)
        else:
            block = self.stack.pop()
            block.create_end_point(timestamp)

        return block

    def add_point(self, timestamp):
        """Finds the deepest block containing this timestamp and creates a
        point within it. If no such block exists, creates a 0-length block to
        contain the point."""

        if not self.tree:
            raise RuntimeError("Call normalize_blocks first")

        interval = self.tree.find_interval(timestamp)
        if not interval or not interval.data:
            block = self.push_block(timestamp)
            self.pop_block(timestamp)
            return block.add_point(timestamp)
        else:
            return interval.data.add_point(timestamp)

    @property
    def all_blocks(self):
        return [self.trace.blocks[idx] for idx in self.blocks]

    def normalize_blocks(self):
        """Aligns blocks without a beginning or an end to the ends of the trace.
        Assigns parent-child relationships to overlapping blocks in this unit.
        """

        for block in self.all_blocks:
            if block.begin is None:
                block.create_begin_point(self.trace.begin)
            if block.end is None:
                block.create_end_point(self.trace.end)

        # Build the interval tree for this unit.
        # We will use this to find appropriate blocks for points as
        # well as the canonical parent-child relationships.
        self.tree = IntervalTree()
        for block in self.all_blocks:
            self.tree.add_interval(
                begin=block.begin_point.timestamp,
                end=block.end_point.timestamp,
                data=block,
            )

        self.__assign_parent_child_blocks(self.tree.root)

    def __assign_parent_child_blocks(self, node):
        if not node:
            return

        for child in node.children:
            if node.data:
                node.data.add_child_block(child.data)
            self.__assign_parent_child_blocks(child)

    def __repr__(self):
        return ("<ExecutionUnit ({id}): \"{name}\"; {blocks} blocks>".format(
            id=self.id,
            name=self.properties.coreProps.get('name', "<None>"),
            blocks=len(self.blocks),
        ))

class Trace(tt.Trace):
    def __init__(self, begin, end, id=None):
        super(Trace, self).__init__(
            id=id,
            executionUnits={},
            blocks={},
            points={},
            version=0,
            edges=[],
            properties=Properties(),
        )
        self.id = id if id else new_id()
        self.begin = begin
        self.end = end

    def add_unit(self):
        unit = ExecutionUnit(self)
        self.executionUnits[unit.id] = unit
        return unit

    def add_edge(self, source, target):
        """Adds an edge between two points."""
        edge = Edge(self, sourcePoint=source.id, targetPoint=target.id)
        self.edges.append(edge)
        return edge
