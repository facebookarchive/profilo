// @generated SignedSource<<b59ea2208f5b4fdce3e6fcdb16da08a4>>

package com.facebook.profilo.entries;

public class EntryType {

  public static final int UNKNOWN_TYPE = 0;
  public static final int UI_INPUT_START = 1;
  public static final int UI_INPUT_END = 2;
  public static final int UI_UPDATE_START = 3;
  public static final int UI_UPDATE_END = 4;
  public static final int NET_ADDED = 5;
  public static final int NET_CANCEL = 6;
  public static final int NET_ERROR = 7;
  public static final int NET_END = 8;
  public static final int NET_RESPONSE = 9;
  public static final int NET_RETRY = 10;
  public static final int NET_START = 11;
  public static final int NET_COUNTER = 12;
  public static final int CALL_START = 13;
  public static final int CALL_END = 14;
  public static final int ASYNC_CALL = 15;
  public static final int SERV_CONN = 16;
  public static final int SERV_DISCONN = 17;
  public static final int SERV_END = 18;
  public static final int ADAPTER_NOTIFY = 19;
  public static final int MARK_FLAG = 20;
  public static final int MARK_PUSH = 21;
  public static final int MARK_POP = 22;
  public static final int LIFECYCLE_APPLICATION_START = 23;
  public static final int LIFECYCLE_APPLICATION_END = 24;
  public static final int LIFECYCLE_ACTIVITY_START = 25;
  public static final int LIFECYCLE_ACTIVITY_END = 26;
  public static final int LIFECYCLE_SERVICE_START = 27;
  public static final int LIFECYCLE_SERVICE_END = 28;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_START = 29;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_END = 30;
  public static final int LIFECYCLE_CONTENT_PROVIDER_START = 31;
  public static final int LIFECYCLE_CONTENT_PROVIDER_END = 32;
  public static final int LIFECYCLE_FRAGMENT_START = 33;
  public static final int LIFECYCLE_FRAGMENT_END = 34;
  public static final int LIFECYCLE_VIEW_START = 35;
  public static final int LIFECYCLE_VIEW_END = 36;
  public static final int TRACE_ABORT = 37;
  public static final int TRACE_END = 38;
  public static final int TRACE_START = 39;
  public static final int TRACE_BACKWARDS = 40;
  public static final int TRACE_TIMEOUT = 41;
  public static final int TRACE_TRIGGER = 42;
  public static final int BLACKBOX_TRACE_START = 43;
  public static final int COUNTER = 44;
  public static final int STACK_FRAME = 45;
  public static final int QPL_START = 46;
  public static final int QPL_END = 47;
  public static final int QPL_CANCEL = 48;
  public static final int QPL_NOTE = 49;
  public static final int QPL_POINT = 50;
  public static final int QPL_EVENT = 51;
  public static final int TRACE_ANNOTATION = 52;
  public static final int WAIT_START = 53;
  public static final int WAIT_END = 54;
  public static final int WAIT_SIGNAL = 55;
  public static final int STRING_KEY = 56;
  public static final int STRING_VALUE = 57;
  public static final int QPL_TAG = 58;
  public static final int QPL_ANNOTATION = 59;
  public static final int TRACE_THREAD_NAME = 60;
  public static final int TRACE_PRE_END = 61;
  public static final int TRACE_THREAD_PRI = 62;
  public static final int MINOR_FAULT = 63;
  public static final int MAJOR_FAULT = 64;
  public static final int PERFEVENTS_LOST = 65;
  public static final int CLASS_LOAD = 66;
  public static final int JAVASCRIPT_STACK_FRAME = 67;
  public static final int MESSAGE_START = 68;
  public static final int MESSAGE_END = 69;
  public static final int CLASS_VALUE = 70;
  public static final int HTTP2_REQUEST_INITIATED = 71;
  public static final int HTTP2_FRAME_HEADER = 72;
  public static final int HTTP2_WINDOW_UPDATE = 73;
  public static final int HTTP2_PRIORITY = 74;
  public static final int HTTP2_EGRESS_FRAME_HEADER = 75;
  public static final int PROCESS_LIST = 76;
  public static final int IO_START = 77;
  public static final int IO_END = 78;
  public static final int CPU_COUNTER = 79;
  public static final int CLASS_LOAD_START = 80;
  public static final int CLASS_LOAD_END = 81;
  public static final int CLASS_LOAD_FAILED = 82;
  public static final int STRING_NAME = 83;
  public static final int JAVA_FRAME_NAME = 84;
  public static final int BINDER_START = 85;
  public static final int BINDER_END = 86;
  public static final int MEMORY_ALLOCATION = 87;
  public static final int MEMORY_DEALLOCATION = 88;
  public static final int STKERR_EMPTYSTACK = 89;
  public static final int STKERR_STACKOVERFLOW = 90;
  public static final int STKERR_NOSTACKFORTHREAD = 91;
  public static final int STKERR_SIGNALINTERRUPT = 92;
  public static final int STKERR_NESTEDUNWIND = 93;
  public static final int STKERR_PARTIALSTACK = 94;
  public static final int STKERR_STACKCOPYFAILED = 95;
  public static final int STKERR_QUEUEOVERFLOW = 96;
  public static final int MAPPING = 97;
  public static final int LOGGER_PRIORITY = 98;
  public static final int CONDITIONAL_UPLOAD_RATE = 99;
  public static final int NATIVE_ALLOC = 100;
  public static final int NATIVE_FREE = 101;
  public static final int NATIVE_ALLOC_FAILURE = 102;
  public static final int NATIVE_STACK_FRAME = 103;
  public static final int NATIVE_FRAME_NAME = 104;
  public static final int MEMORY_MAPPED = 105;
  public static final int MEMORY_UNMAPPED = 106;
  public static final int ION_MEMORY_ALLOC = 107;
  public static final int ION_MEMORY_FREE = 108;
  public static final int ION_MEMORY_MAPPED = 109;
  public static final int GPU_MEMORY_ALLOC = 110;
  public static final int GPU_MEMORY_FREE = 111;
  public static final int THREAD_START = 112;
  public static final int THREAD_FINISH = 113;
  public static final int CPU_STACK_SAMPLE = 114;
  public static final int WALL_STACK_SAMPLE = 115;
  public static final int MEMORY_MAPPING_FAILURE = 116;
  public static final int THREAD_NAMING = 117;
  public static final int STKERR_INVALID_MAP = 118;

  public static final String[] NAMES = {
    "UNKNOWN_TYPE",
    "UI_INPUT_START",
    "UI_INPUT_END",
    "UI_UPDATE_START",
    "UI_UPDATE_END",
    "NET_ADDED",
    "NET_CANCEL",
    "NET_ERROR",
    "NET_END",
    "NET_RESPONSE",
    "NET_RETRY",
    "NET_START",
    "NET_COUNTER",
    "CALL_START",
    "CALL_END",
    "ASYNC_CALL",
    "SERV_CONN",
    "SERV_DISCONN",
    "SERV_END",
    "ADAPTER_NOTIFY",
    "MARK_FLAG",
    "MARK_PUSH",
    "MARK_POP",
    "LIFECYCLE_APPLICATION_START",
    "LIFECYCLE_APPLICATION_END",
    "LIFECYCLE_ACTIVITY_START",
    "LIFECYCLE_ACTIVITY_END",
    "LIFECYCLE_SERVICE_START",
    "LIFECYCLE_SERVICE_END",
    "LIFECYCLE_BROADCAST_RECEIVER_START",
    "LIFECYCLE_BROADCAST_RECEIVER_END",
    "LIFECYCLE_CONTENT_PROVIDER_START",
    "LIFECYCLE_CONTENT_PROVIDER_END",
    "LIFECYCLE_FRAGMENT_START",
    "LIFECYCLE_FRAGMENT_END",
    "LIFECYCLE_VIEW_START",
    "LIFECYCLE_VIEW_END",
    "TRACE_ABORT",
    "TRACE_END",
    "TRACE_START",
    "TRACE_BACKWARDS",
    "TRACE_TIMEOUT",
    "TRACE_TRIGGER",
    "BLACKBOX_TRACE_START",
    "COUNTER",
    "STACK_FRAME",
    "QPL_START",
    "QPL_END",
    "QPL_CANCEL",
    "QPL_NOTE",
    "QPL_POINT",
    "QPL_EVENT",
    "TRACE_ANNOTATION",
    "WAIT_START",
    "WAIT_END",
    "WAIT_SIGNAL",
    "STRING_KEY",
    "STRING_VALUE",
    "QPL_TAG",
    "QPL_ANNOTATION",
    "TRACE_THREAD_NAME",
    "TRACE_PRE_END",
    "TRACE_THREAD_PRI",
    "MINOR_FAULT",
    "MAJOR_FAULT",
    "PERFEVENTS_LOST",
    "CLASS_LOAD",
    "JAVASCRIPT_STACK_FRAME",
    "MESSAGE_START",
    "MESSAGE_END",
    "CLASS_VALUE",
    "HTTP2_REQUEST_INITIATED",
    "HTTP2_FRAME_HEADER",
    "HTTP2_WINDOW_UPDATE",
    "HTTP2_PRIORITY",
    "HTTP2_EGRESS_FRAME_HEADER",
    "PROCESS_LIST",
    "IO_START",
    "IO_END",
    "CPU_COUNTER",
    "CLASS_LOAD_START",
    "CLASS_LOAD_END",
    "CLASS_LOAD_FAILED",
    "STRING_NAME",
    "JAVA_FRAME_NAME",
    "BINDER_START",
    "BINDER_END",
    "MEMORY_ALLOCATION",
    "MEMORY_DEALLOCATION",
    "STKERR_EMPTYSTACK",
    "STKERR_STACKOVERFLOW",
    "STKERR_NOSTACKFORTHREAD",
    "STKERR_SIGNALINTERRUPT",
    "STKERR_NESTEDUNWIND",
    "STKERR_PARTIALSTACK",
    "STKERR_STACKCOPYFAILED",
    "STKERR_QUEUEOVERFLOW",
    "MAPPING",
    "LOGGER_PRIORITY",
    "CONDITIONAL_UPLOAD_RATE",
    "NATIVE_ALLOC",
    "NATIVE_FREE",
    "NATIVE_ALLOC_FAILURE",
    "NATIVE_STACK_FRAME",
    "NATIVE_FRAME_NAME",
    "MEMORY_MAPPED",
    "MEMORY_UNMAPPED",
    "ION_MEMORY_ALLOC",
    "ION_MEMORY_FREE",
    "ION_MEMORY_MAPPED",
    "GPU_MEMORY_ALLOC",
    "GPU_MEMORY_FREE",
    "THREAD_START",
    "THREAD_FINISH",
    "CPU_STACK_SAMPLE",
    "WALL_STACK_SAMPLE",
    "MEMORY_MAPPING_FAILURE",
    "THREAD_NAMING",
    "STKERR_INVALID_MAP",
  };
}
