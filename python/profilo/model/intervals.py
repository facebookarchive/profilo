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


import re
import bisect


def indent(text):
    return re.sub(r'^', '  ', text, flags=re.MULTILINE)


class Interval(object):
    __slots__ = ('begin', 'end', 'data', 'children', '__children_begins')

    def __init__(self, begin, end, data=None):
        assert begin <= end

        self.begin = begin
        self.end = end
        self.data = data
        self.children = []
        self.__children_begins = []  # stores the begin time of every child

    @property
    def length(self):
        return self.end - self.begin

    def add_child(self, interval):
        assert interval in self
        insert_idx = bisect.bisect_right(self.__children_begins, interval.begin)

        self.__children_begins.insert(insert_idx, interval.begin)
        self.children.insert(insert_idx, interval)

    def find_interval(self, interval):
        """Find the smallest containing interval nested under this interval."""

        if not interval in self:
            return None

        begin = interval.begin if isinstance(interval, Interval) else interval

        child_idx = bisect.bisect_right(self.__children_begins, begin)
        if 0 < child_idx <= len(self.children):
            child = self.children[child_idx - 1]
            result = child.find_interval(interval)
            if result:  # child contains the interval
                return result

        # This interval does contain the interval and no children do.
        return self

    def __contains__(self, other):
        if isinstance(other, Interval):
            if self.begin < other.begin < self.end < other.end:
                raise ValueError(
                    "Interval {} (self) overlaps {} in a non-nested way".format(
                        repr(self),
                        repr(other),
                    )
                )
            return self.begin <= other.begin <= other.end <= self.end
        elif isinstance(other, int):
            return self.begin <= other <= self.end
        else:
            raise ValueError("argument must be an Interval or int")

    def __repr__(self):
        if not self.children:
            children_repr = "(none)"
        else:
            children_repr = [repr(child) for child in self.children]
            children_repr = indent("\n".join(children_repr))
            children_repr = "\n{}\n".format(children_repr)

        return "<Interval {} to {} ({}), data: {}, children: {}>".format(
            self.begin,
            self.end,
            self.length,
            repr(self.data),
            children_repr,
        )


class IntervalTree(object):
    """A tree of intervals, with inclusive bounds on either end."""

    def __init__(self):
        self.root = None

    def find_interval(self, number):
        """Find the narrowest interval containing this number."""
        if self.root:
            return self.root.find_interval(number)
        else:
            return None

    def add_interval(self, begin, end, data=None):
        interval = Interval(begin, end, data)
        if not self.root:
            self.root = interval
            return interval

        containing = self.root.find_interval(interval)
        if not containing:
            # There are three options for this interval:
            # 1) Above the root
            # 2) Alongside the root (since the root has data)
            # 3) Extend the root
            if self.root in interval:
                # Above
                interval.add_child(self.root)
                self.root = containing
            elif self.root.data is not None:
                # Alongside
                new_root = Interval(
                    begin=min((interval.begin, self.root.begin)),
                    end=max((interval.end, self.root.end)),
                    data=None,
                )
                new_root.children = [interval, self.root]
                self.root = new_root
            else:
                # Extend the root to contain `interval` as a direct child.
                self.root.begin = min((interval.begin, self.root.begin))
                self.root.end = max((interval.end, self.root.end))
                self.root.add_child(interval)
        else:
            containing.add_child(interval)
        return interval

    def __repr__(self):
        return "<IntervalTree:\n{}\n>".format(indent(repr(self.root)))
