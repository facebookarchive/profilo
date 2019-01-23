// @generated SignedSource<<3aa91c848415dd782836cc932ff83da8>>

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
  public static final int ASYNC_TASK_PRE = 21;
  public static final int ASYNC_TASK_BACKGROUND = 22;
  public static final int ASYNC_TASK_POST = 23;
  public static final int ASYNC_TASK_END = 24;
  public static final int NETWORK_OP_START = 25;
  public static final int NETWORK_OP_END = 26;
  public static final int MARK_FLAG = 27;
  public static final int MARK_START = 28;
  public static final int MARK_STOP = 29;
  public static final int MARK_FAIL = 30;
  public static final int MARK_CANCEL = 31;
  public static final int MARK_PUSH = 32;
  public static final int MARK_POP = 33;
  public static final int LIFECYCLE_APPLICATION_START = 34;
  public static final int LIFECYCLE_APPLICATION_END = 35;
  public static final int LIFECYCLE_ACTIVITY_START = 36;
  public static final int LIFECYCLE_ACTIVITY_END = 37;
  public static final int LIFECYCLE_SERVICE_START = 38;
  public static final int LIFECYCLE_SERVICE_END = 39;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_START = 40;
  public static final int LIFECYCLE_BROADCAST_RECEIVER_END = 41;
  public static final int LIFECYCLE_CONTENT_PROVIDER_START = 42;
  public static final int LIFECYCLE_CONTENT_PROVIDER_END = 43;
  public static final int LIFECYCLE_FRAGMENT_START = 44;
  public static final int LIFECYCLE_FRAGMENT_END = 45;
  public static final int LIFECYCLE_VIEW_START = 46;
  public static final int LIFECYCLE_VIEW_END = 47;
  public static final int TRACE_ABORT = 48;
  public static final int TRACE_END = 49;
  public static final int TRACE_START = 50;
  public static final int TRACE_BACKWARDS = 51;
  public static final int TRACE_TIMEOUT = 52;
  public static final int COUNTER = 53;
  public static final int STACK_FRAME = 54;
  public static final int TEST_CLOCK_SYNC_START = 55;
  public static final int TEST_CLOCK_SYNC_END = 56;
  public static final int QPL_START = 57;
  public static final int QPL_END = 58;
  public static final int QPL_CANCEL = 59;
  public static final int QPL_NOTE = 60;
  public static final int QPL_POINT = 61;
  public static final int TRACE_ANNOTATION = 62;
  public static final int WAIT_START = 63;
  public static final int WAIT_END = 64;
  public static final int WAIT_SIGNAL = 65;
  public static final int STRING_KEY = 66;
  public static final int STRING_VALUE = 67;
  public static final int QPL_TAG = 68;
  public static final int QPL_ANNOTATION = 69;
  public static final int TRACE_THREAD_NAME = 70;
  public static final int TRACE_PRE_END = 71;
  public static final int TRACE_THREAD_PRI = 72;
  public static final int MAJOR_FAULT = 73;
  public static final int YARN_LOST_RECORDS = 74;
  public static final int CLASS_LOAD = 75;
  public static final int JAVASCRIPT_STACK_FRAME = 76;
  public static final int MESSAGE_START = 77;
  public static final int MESSAGE_END = 78;
  public static final int CLASS_VALUE = 79;
  public static final int HTTP2_REQUEST_INITIATED = 80;
  public static final int HTTP2_FRAME_HEADER = 81;
  public static final int HTTP2_WINDOW_UPDATE = 82;
  public static final int HTTP2_PRIORITY = 83;
  public static final int HTTP2_EGRESS_FRAME_HEADER = 84;
  public static final int PROCESS_LIST = 85;
  public static final int IO_START = 86;
  public static final int IO_END = 87;
  public static final int CPU_COUNTER = 88;
  public static final int CLASS_LOAD_START = 89;
  public static final int CLASS_LOAD_END = 90;
  public static final int CLASS_LOAD_FAILED = 91;
  public static final int STRING_NAME = 92;
  public static final int JAVA_FRAME_NAME = 93;

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
  };
}
