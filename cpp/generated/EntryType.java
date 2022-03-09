// @generated SignedSource<<1e08008ef69a4e334fa2515fba4f0495>>

package com.facebook.profilo.entries;

public class EntryType {

  public static final int UNKNOWN_TYPE = 0;
  public static final int UI_INPUT_START = 1;
  public static final int UI_INPUT_END = 2;
  public static final int UI_UPDATE_START = 3;
  public static final int UI_UPDATE_END = 4;
  public static final int NET_ADDED = 5;
  public static final int NET_CANCEL = 6;
  public static final int NET_CHANGEPRI = 7;
  public static final int NET_ERROR = 8;
  public static final int NET_END = 9;
  public static final int NET_RESPONSE = 10;
  public static final int NET_RETRY = 11;
  public static final int NET_START = 12;
  public static final int NET_COUNTER = 13;
  public static final int CALL_START = 14;
  public static final int CALL_END = 15;
  public static final int ASYNC_CALL = 16;
  public static final int SERV_CONN = 17;
  public static final int SERV_DISCONN = 18;
  public static final int SERV_END = 19;
  public static final int ADAPTER_NOTIFY = 20;
  public static final int MARK_FLAG = 21;
  public static final int MARK_PUSH = 22;
  public static final int MARK_POP = 23;
  public static final int LIFECYCLE_APPLICATION_START = 24;
  public static final int LIFECYCLE_APPLICATION_END = 25;
  public static final int LIFECYCLE_ACTIVITY_START = 26;
  public static final int LIFECYCLE_ACTIVITY_END = 27;
  public static final int LIFECYCLE_SERVICE_START = 28;
  public static final int LIFECYCLE_SERVICE_END = 29;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_START = 30;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_END = 31;
  public static final int LIFECYCLE_CONTENT_PROVIDER_START = 32;
  public static final int LIFECYCLE_CONTENT_PROVIDER_END = 33;
  public static final int LIFECYCLE_FRAGMENT_START = 34;
  public static final int LIFECYCLE_FRAGMENT_END = 35;
  public static final int LIFECYCLE_VIEW_START = 36;
  public static final int LIFECYCLE_VIEW_END = 37;
  public static final int TRACE_ABORT = 38;
  public static final int TRACE_END = 39;
  public static final int TRACE_START = 40;
  public static final int TRACE_BACKWARDS = 41;
  public static final int TRACE_TIMEOUT = 42;
  public static final int TRACE_TRIGGER = 43;
  public static final int BLACKBOX_TRACE_START = 44;
  public static final int COUNTER = 45;
  public static final int STACK_FRAME = 46;
  public static final int QPL_START = 47;
  public static final int QPL_END = 48;
  public static final int QPL_CANCEL = 49;
  public static final int QPL_NOTE = 50;
  public static final int QPL_POINT = 51;
  public static final int QPL_EVENT = 52;
  public static final int TRACE_ANNOTATION = 53;
  public static final int WAIT_START = 54;
  public static final int WAIT_END = 55;
  public static final int WAIT_SIGNAL = 56;
  public static final int STRING_KEY = 57;
  public static final int STRING_VALUE = 58;
  public static final int QPL_TAG = 59;
  public static final int QPL_ANNOTATION = 60;
  public static final int TRACE_THREAD_NAME = 61;
  public static final int TRACE_PRE_END = 62;
  public static final int TRACE_THREAD_PRI = 63;
  public static final int MINOR_FAULT = 64;
  public static final int MAJOR_FAULT = 65;
  public static final int PERFEVENTS_LOST = 66;
  public static final int CLASS_LOAD = 67;
  public static final int JAVASCRIPT_STACK_FRAME = 68;
  public static final int MESSAGE_START = 69;
  public static final int MESSAGE_END = 70;
  public static final int CLASS_VALUE = 71;
  public static final int HTTP2_REQUEST_INITIATED = 72;
  public static final int HTTP2_FRAME_HEADER = 73;
  public static final int HTTP2_WINDOW_UPDATE = 74;
  public static final int HTTP2_PRIORITY = 75;
  public static final int HTTP2_EGRESS_FRAME_HEADER = 76;
  public static final int PROCESS_LIST = 77;
  public static final int IO_START = 78;
  public static final int IO_END = 79;
  public static final int CPU_COUNTER = 80;
  public static final int CLASS_LOAD_START = 81;
  public static final int CLASS_LOAD_END = 82;
  public static final int CLASS_LOAD_FAILED = 83;
  public static final int STRING_NAME = 84;
  public static final int JAVA_FRAME_NAME = 85;
  public static final int BINDER_START = 86;
  public static final int BINDER_END = 87;
  public static final int MEMORY_ALLOCATION = 88;
  public static final int MEMORY_DEALLOCATION = 89;
  public static final int STKERR_EMPTYSTACK = 90;
  public static final int STKERR_STACKOVERFLOW = 91;
  public static final int STKERR_NOSTACKFORTHREAD = 92;
  public static final int STKERR_SIGNALINTERRUPT = 93;
  public static final int STKERR_NESTEDUNWIND = 94;
  public static final int STKERR_PARTIALSTACK = 95;
  public static final int STKERR_STACKCOPYFAILED = 96;
  public static final int STKERR_QUEUEOVERFLOW = 97;
  public static final int MAPPING = 98;
  public static final int LOGGER_PRIORITY = 99;
  public static final int CONDITIONAL_UPLOAD_RATE = 100;
  public static final int NATIVE_ALLOC = 101;
  public static final int NATIVE_FREE = 102;
  public static final int NATIVE_ALLOC_FAILURE = 103;
  public static final int NATIVE_STACK_FRAME = 104;
  public static final int NATIVE_FRAME_NAME = 105;
  public static final int MEMORY_MAPPED = 106;
  public static final int MEMORY_UNMAPPED = 107;
  public static final int ION_MEMORY_ALLOC = 108;
  public static final int ION_MEMORY_FREE = 109;
  public static final int ION_MEMORY_MAPPED = 110;
  public static final int GPU_MEMORY_ALLOC = 111;
  public static final int GPU_MEMORY_FREE = 112;
  public static final int THREAD_START = 113;
  public static final int THREAD_FINISH = 114;
  public static final int CPU_STACK_SAMPLE = 115;
  public static final int WALL_STACK_SAMPLE = 116;
  public static final int MEMORY_MAPPING_FAILURE = 117;

  public static final String[] NAMES = {
    "UNKNOWN_TYPE",
    "UI_INPUT_START",
    "UI_INPUT_END",
    "UI_UPDATE_START",
    "UI_UPDATE_END",
    "NET_ADDED",
    "NET_CANCEL",
    "NET_CHANGEPRI",
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
  };
}
