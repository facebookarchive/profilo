// @generated SignedSource<<3b7902e8b87c316cbee21e354df2a62e>>

package com.facebook.profilo.entries;

public class EntryType {

  public static final int UNKNOWN_TYPE = 0;
  public static final int UI_INPUT_START = 1;
  public static final int UI_INPUT_END = 2;
  public static final int UI_UPDATE_START = 3;
  public static final int UI_UPDATE_END = 4;
  public static final int NET_ADDED = 5;
  public static final int NET_ERROR = 6;
  public static final int NET_END = 7;
  public static final int NET_RESPONSE = 8;
  public static final int NET_RETRY = 9;
  public static final int NET_START = 10;
  public static final int NET_COUNTER = 11;
  public static final int CALL_START = 12;
  public static final int CALL_END = 13;
  public static final int ASYNC_CALL = 14;
  public static final int SERV_CONN = 15;
  public static final int SERV_DISCONN = 16;
  public static final int SERV_END = 17;
  public static final int ADAPTER_NOTIFY = 18;
  public static final int ASYNC_TASK_PRE = 19;
  public static final int ASYNC_TASK_BACKGROUND = 20;
  public static final int ASYNC_TASK_POST = 21;
  public static final int ASYNC_TASK_END = 22;
  public static final int NETWORK_OP_START = 23;
  public static final int NETWORK_OP_END = 24;
  public static final int MARK_FLAG = 25;
  public static final int MARK_START = 26;
  public static final int MARK_STOP = 27;
  public static final int MARK_FAIL = 28;
  public static final int MARK_CANCEL = 29;
  public static final int MARK_PUSH = 30;
  public static final int MARK_POP = 31;
  public static final int LIFECYCLE_APPLICATION_START = 32;
  public static final int LIFECYCLE_APPLICATION_END = 33;
  public static final int LIFECYCLE_ACTIVITY_START = 34;
  public static final int LIFECYCLE_ACTIVITY_END = 35;
  public static final int LIFECYCLE_SERVICE_START = 36;
  public static final int LIFECYCLE_SERVICE_END = 37;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_START = 38;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_END = 39;
  public static final int LIFECYCLE_CONTENT_PROVIDER_START = 40;
  public static final int LIFECYCLE_CONTENT_PROVIDER_END = 41;
  public static final int LIFECYCLE_FRAGMENT_START = 42;
  public static final int LIFECYCLE_FRAGMENT_END = 43;
  public static final int LIFECYCLE_VIEW_START = 44;
  public static final int LIFECYCLE_VIEW_END = 45;
  public static final int TRACE_ABORT = 46;
  public static final int TRACE_END = 47;
  public static final int TRACE_START = 48;
  public static final int TRACE_BACKWARDS = 49;
  public static final int TRACE_TIMEOUT = 50;
  public static final int COUNTER = 51;
  public static final int STACK_FRAME = 52;
  public static final int TEST_CLOCK_SYNC_START = 53;
  public static final int TEST_CLOCK_SYNC_END = 54;
  public static final int QPL_START = 55;
  public static final int QPL_END = 56;
  public static final int QPL_CANCEL = 57;
  public static final int QPL_NOTE = 58;
  public static final int QPL_POINT = 59;
  public static final int TRACE_ANNOTATION = 60;
  public static final int WAIT_START = 61;
  public static final int WAIT_END = 62;
  public static final int WAIT_SIGNAL = 63;
  public static final int STRING_KEY = 64;
  public static final int STRING_VALUE = 65;
  public static final int QPL_TAG = 66;
  public static final int QPL_ANNOTATION = 67;
  public static final int TRACE_THREAD_NAME = 68;
  public static final int TRACE_PRE_END = 69;
  public static final int TRACE_THREAD_PRI = 70;
  public static final int MAJOR_FAULT = 71;
  public static final int YARN_LOST_RECORDS = 72;
  public static final int CLASS_LOAD = 73;
  public static final int NATIVE_STACK_FRAME = 74;
  public static final int NATIVE_LIB_MAPPING = 75;
  public static final int JAVASCRIPT_STACK_FRAME = 76;
  public static final int MESSAGE_START = 77;
  public static final int MESSAGE_END = 78;
  public static final int CLASS_VALUE = 79;
  public static final int HTTP2_REQUEST_INITIATED = 80;
  public static final int HTTP2_FRAME_HEADER = 81;
  public static final int HTTP2_WINDOW_UPDATE = 82;
  public static final int HTTP2_PRIORITY = 83;
  public static final int PROCESS_LIST = 84;
  public static final int IO_START = 85;
  public static final int IO_END = 86;
  public static final int CPU_COUNTER = 87;
  public static final int CLASS_LOAD_START = 88;
  public static final int CLASS_LOAD_END = 89;

  public static final String[] NAMES = {
    "UNKNOWN_TYPE",
    "UI_INPUT_START",
    "UI_INPUT_END",
    "UI_UPDATE_START",
    "UI_UPDATE_END",
    "NET_ADDED",
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
    "ASYNC_TASK_PRE",
    "ASYNC_TASK_BACKGROUND",
    "ASYNC_TASK_POST",
    "ASYNC_TASK_END",
    "NETWORK_OP_START",
    "NETWORK_OP_END",
    "MARK_FLAG",
    "MARK_START",
    "MARK_STOP",
    "MARK_FAIL",
    "MARK_CANCEL",
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
    "TEST_CLOCK_SYNC_START",
    "TEST_CLOCK_SYNC_END",
    "QPL_START",
    "QPL_END",
    "QPL_CANCEL",
    "QPL_NOTE",
    "QPL_POINT",
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
    "MAJOR_FAULT",
    "YARN_LOST_RECORDS",
    "CLASS_LOAD",
    "NATIVE_STACK_FRAME",
    "NATIVE_LIB_MAPPING",
    "JAVASCRIPT_STACK_FRAME",
    "MESSAGE_START",
    "MESSAGE_END",
    "CLASS_VALUE",
    "HTTP2_REQUEST_INITIATED",
    "HTTP2_FRAME_HEADER",
    "HTTP2_WINDOW_UPDATE",
    "HTTP2_PRIORITY",
    "PROCESS_LIST",
    "IO_START",
    "IO_END",
    "CPU_COUNTER",
    "CLASS_LOAD_START",
    "CLASS_LOAD_END",
  };
}
