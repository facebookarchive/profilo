// @generated SignedSource<<776706c631f555de07373bf9a30ba723>>

#pragma once

namespace facebook {
namespace profilo {
namespace entries {
enum class EntryType {
  UNKNOWN_TYPE = 0,
  UI_INPUT_START = 1,
  UI_INPUT_END = 2,
  UI_UPDATE_START = 3,
  UI_UPDATE_END = 4,
  NET_ADDED = 5,
  NET_CANCEL = 6,
  NET_CHANGEPRI = 7,
  NET_ERROR = 8,
  NET_END = 9,
  NET_RESPONSE = 10,
  NET_RETRY = 11,
  NET_START = 12,
  NET_COUNTER = 13,
  CALL_START = 14,
  CALL_END = 15,
  ASYNC_CALL = 16,
  SERV_CONN = 17,
  SERV_DISCONN = 18,
  SERV_END = 19,
  ADAPTER_NOTIFY = 20,
  MARK_FLAG = 21,
  MARK_PUSH = 22,
  MARK_POP = 23,
  LIFECYCLE_APPLICATION_START = 24,
  LIFECYCLE_APPLICATION_END = 25,
  LIFECYCLE_ACTIVITY_START = 26,
  LIFECYCLE_ACTIVITY_END = 27,
  LIFECYCLE_SERVICE_START = 28,
  LIFECYCLE_SERVICE_END = 29,
  LIFECYCLE_BROADCAST_RECEIVER_START = 30,
  LIFECYCLE_BROADCAST_RECEIVER_END = 31,
  LIFECYCLE_CONTENT_PROVIDER_START = 32,
  LIFECYCLE_CONTENT_PROVIDER_END = 33,
  LIFECYCLE_FRAGMENT_START = 34,
  LIFECYCLE_FRAGMENT_END = 35,
  LIFECYCLE_VIEW_START = 36,
  LIFECYCLE_VIEW_END = 37,
  TRACE_ABORT = 38,
  TRACE_END = 39,
  TRACE_START = 40,
  TRACE_BACKWARDS = 41,
  TRACE_TIMEOUT = 42,
  TRACE_TRIGGER = 43,
  BLACKBOX_TRACE_START = 44,
  COUNTER = 45,
  STACK_FRAME = 46,
  QPL_START = 47,
  QPL_END = 48,
  QPL_CANCEL = 49,
  QPL_NOTE = 50,
  QPL_POINT = 51,
  QPL_EVENT = 52,
  TRACE_ANNOTATION = 53,
  WAIT_START = 54,
  WAIT_END = 55,
  WAIT_SIGNAL = 56,
  STRING_KEY = 57,
  STRING_VALUE = 58,
  QPL_TAG = 59,
  QPL_ANNOTATION = 60,
  TRACE_THREAD_NAME = 61,
  TRACE_PRE_END = 62,
  TRACE_THREAD_PRI = 63,
  MINOR_FAULT = 64,
  MAJOR_FAULT = 65,
  PERFEVENTS_LOST = 66,
  CLASS_LOAD = 67,
  JAVASCRIPT_STACK_FRAME = 68,
  MESSAGE_START = 69,
  MESSAGE_END = 70,
  CLASS_VALUE = 71,
  HTTP2_REQUEST_INITIATED = 72,
  HTTP2_FRAME_HEADER = 73,
  HTTP2_WINDOW_UPDATE = 74,
  HTTP2_PRIORITY = 75,
  HTTP2_EGRESS_FRAME_HEADER = 76,
  PROCESS_LIST = 77,
  IO_START = 78,
  IO_END = 79,
  CPU_COUNTER = 80,
  CLASS_LOAD_START = 81,
  CLASS_LOAD_END = 82,
  CLASS_LOAD_FAILED = 83,
  STRING_NAME = 84,
  JAVA_FRAME_NAME = 85,
  BINDER_START = 86,
  BINDER_END = 87,
  MEMORY_ALLOCATION = 88,
  MEMORY_DEALLOCATION = 89,
  STKERR_EMPTYSTACK = 90,
  STKERR_STACKOVERFLOW = 91,
  STKERR_NOSTACKFORTHREAD = 92,
  STKERR_SIGNALINTERRUPT = 93,
  STKERR_NESTEDUNWIND = 94,
  STKERR_PARTIALSTACK = 95,
  STKERR_STACKCOPYFAILED = 96,
  STKERR_QUEUEOVERFLOW = 97,
  MAPPING = 98,
  LOGGER_PRIORITY = 99,
  CONDITIONAL_UPLOAD_RATE = 100,
  NATIVE_ALLOC = 101,
  NATIVE_FREE = 102,
  NATIVE_ALLOC_FAILURE = 103,
  NATIVE_STACK_FRAME = 104,
  NATIVE_FRAME_NAME = 105,
  MEMORY_MAPPED = 106,
  MEMORY_UNMAPPED = 107,
  ION_MEMORY_ALLOC = 108,
  ION_MEMORY_FREE = 109,
  ION_MEMORY_MAPPED = 110,
  GPU_MEMORY_ALLOC = 111,
  GPU_MEMORY_FREE = 112,
  THREAD_START = 113,
  THREAD_FINISH = 114,
  CPU_STACK_SAMPLE = 115,
  WALL_STACK_SAMPLE = 116,
  MEMORY_MAPPING_FAILURE = 117,
};


const char* to_string(EntryType type);
} // namespace entries
} // namespace profilo
} // namespace facebook
