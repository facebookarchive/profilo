// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.core;

public final class ProfiloConstants {

  public static final int PROVIDER_PROFILO_SYSTEM = 0xFFFFFFFF;

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
