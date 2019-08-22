// @generated SignedSource<<16801a21cc20c06cd8c6657d68a9238d>>

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
  public static final int COUNTER = 43;
  public static final int STACK_FRAME = 44;
  public static final int QPL_START = 45;
  public static final int QPL_END = 46;
  public static final int QPL_CANCEL = 47;
  public static final int QPL_NOTE = 48;
  public static final int QPL_POINT = 49;
  public static final int QPL_EVENT = 50;
  public static final int TRACE_ANNOTATION = 51;
  public static final int WAIT_START = 52;
  public static final int WAIT_END = 53;
  public static final int WAIT_SIGNAL = 54;
  public static final int STRING_KEY = 55;
  public static final int STRING_VALUE = 56;
  public static final int QPL_TAG = 57;
  public static final int QPL_ANNOTATION = 58;
  public static final int TRACE_THREAD_NAME = 59;
  public static final int TRACE_PRE_END = 60;
  public static final int TRACE_THREAD_PRI = 61;
  public static final int MINOR_FAULT = 62;
  public static final int MAJOR_FAULT = 63;
  public static final int PERFEVENTS_LOST = 64;
  public static final int CLASS_LOAD = 65;
  public static final int JAVASCRIPT_STACK_FRAME = 66;
  public static final int MESSAGE_START = 67;
  public static final int MESSAGE_END = 68;
  public static final int CLASS_VALUE = 69;
  public static final int HTTP2_REQUEST_INITIATED = 70;
  public static final int HTTP2_FRAME_HEADER = 71;
  public static final int HTTP2_WINDOW_UPDATE = 72;
  public static final int HTTP2_PRIORITY = 73;
  public static final int HTTP2_EGRESS_FRAME_HEADER = 74;
  public static final int PROCESS_LIST = 75;
  public static final int IO_START = 76;
  public static final int IO_END = 77;
  public static final int CPU_COUNTER = 78;
  public static final int CLASS_LOAD_START = 79;
  public static final int CLASS_LOAD_END = 80;
  public static final int CLASS_LOAD_FAILED = 81;
  public static final int STRING_NAME = 82;
  public static final int JAVA_FRAME_NAME = 83;
  public static final int BINDER_START = 84;
  public static final int BINDER_END = 85;
  public static final int MEMORY_ALLOCATION = 86;
  public static final int STKERR_EMPTYSTACK = 87;
  public static final int STKERR_STACKOVERFLOW = 88;
  public static final int STKERR_NOSTACKFORTHREAD = 89;
  public static final int STKERR_SIGNALINTERRUPT = 90;
  public static final int STKERR_NESTEDUNWIND = 91;
  public static final int MAPPING = 92;

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
    "STKERR_EMPTYSTACK",
    "STKERR_STACKOVERFLOW",
    "STKERR_NOSTACKFORTHREAD",
    "STKERR_SIGNALINTERRUPT",
    "STKERR_NESTEDUNWIND",
    "MAPPING",
  };
}
