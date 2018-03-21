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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from ..codegen import EntryDescription
from ..codegen import MemoryDescription
from ..types import Types
from ..types import DynamicArrayType

NAMES = [
    'UNKNOWN_TYPE',

    'UI_INPUT_START',
    'UI_INPUT_END',

    'UI_UPDATE_START',
    'UI_UPDATE_END',

    'NET_ADDED',
    'NET_ERROR',
    'NET_END',
    'NET_RESPONSE',
    'NET_RETRY',
    'NET_START',
    'NET_COUNTER',

    'CALL_START',
    'CALL_END',

    'ASYNC_CALL',

    'SERV_CONN',
    'SERV_DISCONN',
    'SERV_END',

    'ADAPTER_NOTIFY',

    'ASYNC_TASK_PRE',
    'ASYNC_TASK_BACKGROUND',
    'ASYNC_TASK_POST',
    'ASYNC_TASK_END',
    'NETWORK_OP_START',
    'NETWORK_OP_END',

    'MARK_FLAG',
    'MARK_START',
    'MARK_STOP',
    'MARK_FAIL',
    'MARK_CANCEL',
    'MARK_PUSH',
    'MARK_POP',

    'LIFECYCLE_APPLICATION_START',
    'LIFECYCLE_APPLICATION_END',

    'LIFECYCLE_ACTIVITY_START',
    'LIFECYCLE_ACTIVITY_END',

    'LIFECYCLE_SERVICE_START',
    'LIFECYCLE_SERVICE_END',

    'LIFECYCLE_BROADCAST_RECEIVER_START',
    'LIFECYCLE_BROADCAST_RECEIVER_END',

    'LIFECYCLE_CONTENT_PROVIDER_START',
    'LIFECYCLE_CONTENT_PROVIDER_END',

    'LIFECYCLE_FRAGMENT_START',
    'LIFECYCLE_FRAGMENT_END',

    'LIFECYCLE_VIEW_START',
    'LIFECYCLE_VIEW_END',

    'TRACE_ABORT',
    'TRACE_END',
    'TRACE_START',
    'TRACE_BACKWARDS',
    'TRACE_TIMEOUT',

    'COUNTER',
    'STACK_FRAME',

    'TEST_CLOCK_SYNC_START',
    'TEST_CLOCK_SYNC_END',

    'QPL_START',
    'QPL_END',
    'QPL_CANCEL',
    'QPL_NOTE',
    'QPL_POINT',

    'TRACE_ANNOTATION',

    'WAIT_START',
    'WAIT_END',
    'WAIT_SIGNAL',

    'STRING_KEY',
    'STRING_VALUE',

    'QPL_TAG',
    'QPL_ANNOTATION',

    'TRACE_THREAD_NAME',

    'TRACE_PRE_END',
    'TRACE_THREAD_PRI',
    'MAJOR_FAULT',
    'YARN_LOST_RECORDS',

    'CLASS_LOAD',
    'NATIVE_STACK_FRAME',
    'NATIVE_LIB_MAPPING',

    'JAVASCRIPT_STACK_FRAME',

    'MESSAGE_START',
    'MESSAGE_END',
    'CLASS_VALUE',

    'HTTP2_REQUEST_INITIATED',
    'HTTP2_FRAME_HEADER',
    'HTTP2_WINDOW_UPDATE',
    'HTTP2_PRIORITY',

    'PROCESS_LIST',

    'IO_START',
    'IO_END',

    'CPU_COUNTER',
]

STACK_FRAME_ENTRIES = frozenset([
    'STACK_FRAME',
    'NATIVE_STACK_FRAME',
    'JAVASCRIPT_STACK_FRAME',
])

BYTES_ENTRIES = frozenset([
    'STRING_KEY',
    'STRING_VALUE',
])

TARGET_SIZE = (
    128 -
    Types.int32.constant_size -  # ring buffer slot overhead
    Types.int32.constant_size -  # stream ID
    Types.int8.constant_size -   # size + next flag
    Types.int8.constant_size # serialization format
)

def get_frames_memory_format():
    fields = [
        ('id', Types.int32),
        ('type', Types.uint8),
        ('timestamp', Types.int64),
        ('tid', Types.int32),
        ('frames', DynamicArrayType(Types.int64)),
    ]

    return MemoryDescription(
        fields=fields,
        typename='FramesEntry',
    )

def get_bytes_memory_format():
    fields = [
        ('id', Types.int32),
        ('type', Types.uint8),
        ('matchid', Types.int32),
        ('bytes', DynamicArrayType(Types.uint8)),
    ]

    return MemoryDescription(
        fields=fields,
        typename='BytesEntry',
    )


def get_entry_descriptions():
    descriptions = []
    standard_entry = MemoryDescription(
        fields=[
            ('id', Types.int32),
            ('type', Types.uint8),
            ('timestamp', Types.int64),
            ('tid', Types.int32),
            ('callid', Types.int32),
            ('matchid', Types.int32),
            ('extra', Types.int64),
        ],
        typename='StandardEntry',
    )

    frames_entry = get_frames_memory_format()
    bytes_entry = get_bytes_memory_format()

    for idx, name in enumerate(NAMES):
        if name in STACK_FRAME_ENTRIES:
            memory_format = frames_entry
        elif name in BYTES_ENTRIES:
            memory_format = bytes_entry
        else:
            memory_format = standard_entry

        descriptions.append(
            EntryDescription(
                id=idx,
                name=name,
                memory_format=memory_format,
            ),
        )

    return descriptions
