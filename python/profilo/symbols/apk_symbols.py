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


import struct
import sys
import zipfile
import tempfile
from collections import namedtuple

DEX_FILE_VERSION_MAX = 38


def parse_dex_id(signature):
    """Take first 4 bytes of the Dex signature as Dex Id """
    return struct.unpack("<I", signature[0:4])[0]


def parse_type_signature(s):
    primitives = {
        ord('V'): b'void',
        ord('Z'): b'boolean',
        ord('B'): b'byte',
        ord('S'): b'short',
        ord('C'): b'char',
        ord('I'): b'int',
        ord('J'): b'long',
        ord('F'): b'float',
        ord('D'): b'double',
    }
    array_order = 0
    type_name = None
    for i, c in enumerate(s):
        if c in primitives:
            type_name = primitives[c]
        elif c == b'[':
            array_order += 1
        elif c == ord('L'):
            type_name = s[i + 1:-1].replace(b'/', b'.')
            break
    try:
        type_name = type_name.decode("ascii")
        return type_name + '[]' * array_order
    except UnicodeDecodeError:
        # Non-ASCII == garbage
        return None


def uleb128_decode(f):
    """Decode Unsigned LEB128

    http://en.wikipedia.org/wiki/LEB128#Decode_unsigned_integer"""
    result = 0
    shift = 0
    while True:
        b = ord(f.read(1))
        result |= (b & 0x7f) << shift
        if b & 0x80 == 0:
            break
        shift += 7
    return result


class StructReader(struct.Struct):
    """Struct helper with attributes"""
    def __init__(self, prefix, field_spec, f=None):
        self.__fmt = prefix
        self.__indices = {}
        self.__values = None
        self.__field_spec = field_spec
        for i, (field_name, field_type) in enumerate(field_spec):
            self.__fmt += field_type
            self.__indices[field_name] = i
        struct.Struct.__init__(self, str(self.__fmt))
        if f:
            self.read(f)

    def read(self, f):
        self.__values = self.unpack(f.read(self.size))
        return self

    def __getattr__(self, name):
        if name in self.__indices:
            return self.__values[self.__indices[name]]
        else:
            raise AttributeError(name)

    @classmethod
    def get(cls, field_spec, prefix='<'):
        return lambda f: cls(prefix, field_spec, f)


DexHeader = StructReader.get([
    ('dex_file_magic', '8s'),
    ('checksum', 'I'),
    ('signature', '20s'),
    ('file_size', 'I'),
    ('header_size', 'I'),
    ('endian_tag', 'I'),
    ('link_size', 'I'),
    ('link_off', 'I'),
    ('map_off', 'I'),
    ('string_ids_size', 'I'),
    ('string_ids_off', 'I'),
    ('type_ids_size', 'I'),
    ('type_ids_off', 'I'),
    ('proto_ids_size', 'I'),
    ('proto_ids_off', 'I'),
    ('field_ids_size', 'I'),
    ('field_ids_off', 'I'),
    ('method_ids_size', 'I'),
    ('method_ids_off', 'I'),
    ('class_defs_size', 'I'),
    ('class_defs_off', 'I'),
    ('data_size', 'I'),
    ('data_off', 'I'),
])

MethodIdItem = StructReader.get([
    ('class_idx', 'H'),
    ('proto_idx', 'H'),
    ('name_idx', 'I'),
])

StringIdItem = StructReader.get([('string_data_off', 'I')])

TypeIdItem = StructReader.get([('descriptor_idx', 'I')])


class StringDataItem(object):
    def __init__(self, f):
        # This doesn't really handle the MUTF-8 encoding,
        # but should be okay for class and method names in our apps.
        self.utf16_size = uleb128_decode(f)
        self.data = bytes()
        while True:
            c = f.read(1)
            if ord(c) == 0:
                return
            else:
                self.data += c


class Dex(object):
    """Partial representation of a Dex file"""

    def __init__(self, f):
        self.header = DexHeader(f)
        dex_version = int(self.header.dex_file_magic[4:7])
        assert(dex_version <= DEX_FILE_VERSION_MAX)
        self.dex_id = parse_dex_id(self.header.signature)
        self.bytes = self._read_bytes(f)
        self.method_ids = self._read(
            f, self.header.method_ids_off, self.header.method_ids_size, MethodIdItem)
        self.type_ids = self._read(
            f, self.header.type_ids_off, self.header.type_ids_size, TypeIdItem)

    def _read_bytes(self, f):
        offsets = []
        f.seek(self.header.string_ids_off)
        for _ in range(self.header.string_ids_size):
            offsets.append(StringIdItem(f).string_data_off)
        result = []
        for i in range(self.header.string_ids_size):
            f.seek(offsets[i])
            result.append(StringDataItem(f).data)
        return result

    def _read(self, f, offset, count, obj):
        result = []
        f.seek(offset)
        for idx in range(count):
            result.append((idx, obj(f)))
        return result

    def _global_id(self, idx):
        return (idx << 32) | self.dex_id

    def get_methods_map(self):
        """Returns a dictionary with all methods in the Dex file """
        methods_map = {}
        for method_idx, m in self.method_ids:
            class_name = parse_type_signature(
                self.bytes[self.type_ids[m.class_idx][1].descriptor_idx])
            if not class_name:
                continue
            try:
                method_name = self.bytes[m.name_idx].decode("ascii")
            except UnicodeDecodeError:
                continue
            methods_map[self._global_id(method_idx)] = \
                "{}::{}".format(class_name, method_name)
        return methods_map

    def get_classes_map(self):
        """Returns a dictionary with all classes in the Dex file """
        classes_map = {}
        for class_idx, t in self.type_ids:
            class_name = parse_type_signature(self.bytes[t.descriptor_idx])
            if not class_name:
                continue
            classes_map[self._global_id(class_idx)] = class_name
        return classes_map


def extract_apk_symbols(apk_path):
    """Locates primary and secondary Dex files in an apk, extracts and returns
    consolidated classes and methods mappings from every dex.

    The returned dictionary format:
    {
        "class_index" : {
            ((class_id << 32) | dex_id) : "<class name>"
            ...
        },
        "method_index" : {
            ((method_id << 32) | dex_id) : "<class name>"
            ...
        }
    }
    """
    class_index = {}
    method_index = {}

    with zipfile.ZipFile(apk_path) as apk_file:
        tempdir = tempfile.mkdtemp()
        assert("classes.dex" in apk_file.namelist())
        index = 1
        dex_file_format = "classes{}.dex"
        dex_file = dex_file_format.format("")
        while dex_file in apk_file.namelist():
            dex_path = apk_file.extract(dex_file, tempdir)
            with open(dex_path, 'rb') as dex_file:
                dex = Dex(dex_file)
                class_index.update(dex.get_classes_map())
                method_index.update(dex.get_methods_map())
            index += 1
            dex_file = dex_file_format.format(index)

    return namedtuple('ApkSymbols', ['class_index', 'method_index'])(class_index, method_index)


if __name__ == '__main__':
    import json
    apk_path = sys.argv[1]
    symbols = extract_apk_symbols(apk_path)
    print(json.dumps({"class_index": symbols.class_index, "method_index": symbols.method_index, }))
