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


from collections import namedtuple

class TraceEntry(object):
    @staticmethod
    def construct(line):
        line = line.split('|')
        if line[1] in ['STRING_KEY', 'STRING_VALUE']:
            return BytesEntry(
                id=int(line[0]),
                type=line[1],
                arg1=int(line[2]),
                data=line[3],
            )
        else:
            return StandardEntry(
                id=int(line[0]),
                type=line[1],
                timestamp=int(line[2]),
                tid=int(line[3]),
                arg1=int(line[4]),
                arg2=int(line[5]),
                arg3=int(line[6]),
            )

class StandardEntry(TraceEntry,
    namedtuple(
        'StandardEntry',
        ['id', 'type', 'timestamp', 'tid', 'arg1', 'arg2', 'arg3'],
    )):
    pass

class BytesEntry(TraceEntry,
    namedtuple(
        'BytesEntry',
        ['id', 'type', 'arg1', 'data'],
    )):
    pass

class TraceFile(object):

    def __init__(self, headers={}, entries=[]):
        super(TraceFile, self).__init__()
        self.headers = headers
        self.entries = entries

    @staticmethod
    def __do64bitAddition(a, b):
        val = a + b
        if not -0x8000000000000000 <= val <= 0x7fffffffffffffff:
            val = (val + (0x8000000000000000)) % (2 * (0x8000000000000000)) - 0x8000000000000000
        return val

    @staticmethod
    def __do32bitAddition(a, b):
        val = a + b
        if not -0x80000000 <= val <= 0x7fffffff:
            val = (val + (0x80000000)) % (2 * (0x80000000)) - 0x80000000
        return val

    @staticmethod
    def __delta_decode_entries(headers, delta_encoded):
        # Timestamp precision for all standard entry timestamps is in the
        # headers. Maximum precision is 9 for 10^-9, i.e. nanoseconds.
        precision = int(headers.get('prec', 0))
        # Figure out the multiplication factor from the precision to nanos.
        timestamp_multiplier = pow(10, (9 - precision))

        entries = []
        last_entry = None
        for entry in delta_encoded:
            if not isinstance(entry, StandardEntry):
                entries.append(entry)
                continue

            if not last_entry:
                # First entry is not delta-encoded but timestamp may
                # still be truncated.
                id, type, timestamp, tid, arg1, arg2, arg3 = entry
                adjusted_entry = StandardEntry(
                    id=id,
                    type=type,
                    timestamp=(timestamp * timestamp_multiplier),
                    tid=tid,
                    arg1=arg1,
                    arg2=arg2,
                    arg3=arg3,
                )
                entries.append(adjusted_entry)
                last_entry = adjusted_entry
                continue

            delta_entry = StandardEntry(
                id=TraceFile.__do32bitAddition(last_entry.id, entry.id),
                type=entry.type,
                timestamp=TraceFile.__do64bitAddition(last_entry.timestamp, (entry.timestamp * timestamp_multiplier)),
                tid=TraceFile.__do32bitAddition(last_entry.tid, entry.tid),
                arg1=TraceFile.__do32bitAddition(last_entry.arg1, entry.arg1),
                arg2=TraceFile.__do32bitAddition(last_entry.arg2, entry.arg2),
                arg3=TraceFile.__do64bitAddition(last_entry.arg3, entry.arg3)
            )
            entries.append(delta_entry)
            last_entry = delta_entry
        return entries

    @staticmethod
    def from_string(data):

        # Headers are separated from actual data by '\n\n'.
        data = data.split("\n\n", 1)

        # Headers have a `key|value` format.
        headers = [x.split("|") for x in data[0].split("\n")]
        headers = {x[0]: x[1] for x in headers if len(x) >= 2}
        data = data[1]

        # Don't materialize the full list of delta-encoded entries,
        # generate them on demand.
        gen_entries = (TraceEntry.construct(line) for line in data.split("\n") if len(line.strip()) > 0)
        entries = TraceFile.__delta_decode_entries(headers, gen_entries)

        return TraceFile(headers=headers, entries=entries)

    @staticmethod
    def from_file(fd):
        with fd:
            data = fd.read().decode("utf-8")
            return TraceFile.from_string(data)

if __name__ == "__main__":
    import sys
    import gzip
    with gzip.open(sys.argv[1]) as f:
        trace = TraceFile.from_file(f)
        for entry in trace.entries:
            print(entry)
