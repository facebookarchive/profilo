// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

final public class LoomConstants {

  public static final int PROVIDER_ASYNC =           1;
  public static final int PROVIDER_LIFECYCLE =       1 << 1;
  public static final int PROVIDER_QPL =             1 << 2;
  public static final int PROVIDER_OTHER =           1 << 3;
  public static final int PROVIDER_USER_COUNTERS =   1 << 4;
  public static final int PROVIDER_SYSTEM_COUNTERS = 1 << 5;
  public static final int PROVIDER_STACK_FRAME =     1 << 6;
  public static final int PROVIDER_LIGER =           1 << 7;
  public static final int PROVIDER_MAJOR_FAULTS =    1 << 8;
  public static final int PROVIDER_THREAD_SCHEDULE = 1 << 9;
  public static final int PROVIDER_CLASS_LOAD =      1 << 10;
  public static final int PROVIDER_NATIVE_STACK_TRACE =    1 << 11;
  public static final int PROVIDER_JAVASCRIPT_STACK_TRACE =  1 << 12;
  public static final int PROVIDER_MAIN_THREAD_MESSAGES =  1 << 13;
  public static final int PROVIDER_LIGER_HTTP2 = 1 << 14;
  public static final int PROVIDER_FBSYSTRACE = 1 << 15;
  public static final int PROVIDER_HIGH_FREQ_MAIN_THREAD_COUNTERS = 1 << 16;
  public static final int PROVIDER_TRANSIENT_NETWORK_DATA = 1 << 17;
  public static final int PROVIDER_WALL_TIME_STACK_TRACE = 1 << 18;
  public static final int PROVIDER_LIBC_IO = 1 << 19;
  public static final int PROVIDER_UI_THREAD_RESPONSIVENESS = 1 << 20;

  public static final int PROVIDER_LOOM_SYSTEM = 0xFFFFFFFF;

  public static final int TRIGGER_QPL =             1;
  public static final int TRIGGER_MANUAL =          1 << 1;
  public static final int TRIGGER_COLD_START =      1 << 2;
  public static final int TRIGGER_SEQUENCE_QPL =    1 << 3;
  public static final int TRIGGER_MULTI_PROCESS =   1 << 4;
  public static final int TRIGGER_EXTERNAL = 1 << 5;
  public static final int TRIGGER_BLACK_BOX = 1 << 6;
  public static final int TRIGGER_VIDEO_PROFILER = 1 << 7;
  public static final int TRIGGER_LITE = 1 << 8;

  public static final int NO_CALLSITE = 0;
  public static final int NO_MATCH = 0;
  public static final int TRACING_DISABLED = -1;

  public static final String STRING_KEY_BLOCK_NAME = "__name";

  /* Keep in sync with AbortReason.h */
  public static final int ABORT_REASON_UNKNOWN = 1;
  public static final int ABORT_REASON_CONTROLLER_INITIATED = 2;
  public static final int ABORT_REASON_MISSED_EVENT = 3;
  public static final int ABORT_REASON_TIMEOUT = 4;
  public static final int ABORT_REASON_NEW_START = 5;
  public static final int ABORT_REASON_REMOTE_PROCESS = 6;
}
