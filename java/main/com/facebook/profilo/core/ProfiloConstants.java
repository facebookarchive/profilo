/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.facebook.profilo.core;

public final class ProfiloConstants {

  public static final int NONE = 0;
  public static final int NO_CALLSITE = 0;
  public static final int NO_MATCH = 0;
  public static final int TRACING_DISABLED = -1;

  /* Keep in sync with AbortReason.h */
  public static final int ABORT_REASON_UNKNOWN = 1;
  public static final int ABORT_REASON_CONTROLLER_INITIATED = 2;
  public static final int ABORT_REASON_MISSED_EVENT = 3;
  public static final int ABORT_REASON_TIMEOUT = 4;
  public static final int ABORT_REASON_NEW_START = 5;
  public static final int ABORT_REASON_CONDITION_NOT_MET = 6;
  public static final int ABORT_REASON_WRITER_EXCEPTION = 8;
  public static final int ABORT_REASON_LOGOUT = 9;

  // Things in the remote process can go wrong for the same reason as in the
  // main process. Thus, just mark if the reason is "remote" by using a single
  // bit
  public static final int REMOTE_PROCESS_BIT = 0x80000000;

  // Sampling rate config parameter for the call stacks provider.
  public static final String CPU_SAMPLING_RATE_CONFIG_PARAM =
      "provider.stack_trace.cpu_sampling_rate_ms";
  public static final String SYSTEM_CONFIG_TIMED_OUT_UPLOAD_SAMPLE_RATE =
      "system_config.timed_out_upload_sample_rate";
  public static final String TIME_SOURCE_PARAM = "provider.stack_trace.time_source";
  public static final String TRACE_CONFIG_COINFLIP_SAMPLE_RATE =
      "trace_config.coinflip_sample_rate";
  public static final String TRACE_CONFIG_TRACE_CONFIG_ID_SWITCH =
      "trace_config.trace_config_id_switch";
  public static final String TRACE_CONFIG_ID = "trace_config.id";

  public static int packRemoteAbortReason(int abortReason) {
    return (abortReason | REMOTE_PROCESS_BIT);
  }

  public static int unpackRemoteAbortReason(int remoteAbortReason) {
    return (remoteAbortReason & ~REMOTE_PROCESS_BIT);
  }

  public static boolean isRemoteAbort(int abortReason) {
    return (abortReason & REMOTE_PROCESS_BIT) == REMOTE_PROCESS_BIT;
  }

  public static String abortReasonName(int abortReason) {
    switch (abortReason) {
      case ABORT_REASON_UNKNOWN:
        return "unknown";
      case ABORT_REASON_CONTROLLER_INITIATED:
        return "controller_init";
      case ABORT_REASON_MISSED_EVENT:
        return "missed_event";
      case ABORT_REASON_TIMEOUT:
        return "timeout";
      case ABORT_REASON_NEW_START:
        return "new_start";
      case ABORT_REASON_CONDITION_NOT_MET:
        return "condition_not_met";
      case ABORT_REASON_WRITER_EXCEPTION:
        return "writer_exception";
      case ABORT_REASON_LOGOUT:
        return "logout";
    }
    return "UNKNOWN REASON " + abortReason;
  }

  public static final String TRACE_CONFIG_PARAM_TRACE_TIMEOUT_MS = "trace_config.trace_timeout_ms";
  public static final String TRACE_CONFIG_PARAM_LOGGER_PRIORITY = "trace_config.logger_priority";
  public static final int TRACE_CONFIG_PARAM_LOGGER_PRIORITY_DEFAULT = 5;
  public static final String TRACE_CONFIG_PARAM_POST_TRACE_EXTENSION_MSEC =
      "trace_config.post_trace_extension_ms";
  public static final int TRACE_CONFIG_PARAM_POST_TRACE_EXTENSION_MSEC_DEFAULT = 0;
  public static final String TRACE_CONFIG_PARAM_BLACKBOX_PAUSE_IN_BG =
      "trace_config.should_pause_in_background";
  public static final String PROVIDER_PARAM_STACK_TRACE_THREAD_DETECT_INTERVAL_MS =
      "provider.stack_trace.thread_detect_interval_ms";
  public static final String PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWIND_DEX_FRAMES =
      "provider.native_stack_trace.unwind_dex_frames";
  public static final String PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWIND_JIT_FRAMES =
      "provider.native_stack_trace.unwind_jit_frames";
  public static final String PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWINDER_THREAD_PRIORITY =
      "provider.native_stack_trace.unwinder_thread_pri";
  public static final String PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWINDER_QUEUE_SIZE =
      "provider.native_stack_trace.unwinder_queue_size";
  public static final int PROVIDER_PARAM_NATIVE_STACK_TRACE_UNWINDER_QUEUE_SIZE_DEFAULT = 256;
  public static final String PROVIDER_PARAM_NATIVE_STACK_TRACE_LOG_PARTIAL_STACKS =
      "provider.native_stack_trace.log_partial_stacks";

  // Keys to query conditions in a config
  public static final String TRACE_CONFIG_DURATION_CONDITION = "trace_config.duration_condition";
  public static final String TRACE_CONFIG_STRING_LIST_CONDITION =
      "trace_config.string_list_condition";
  public static final String TRACE_CONFIG_STRING_LIST_CONDITION_FALLBACK_SAMPLING_RATE =
      "trace_config.fallback_sampling_rate_for_string_list_condition";

  // Constants for max upload sampling rate (1:1) and for no upload (0)
  public static final int UPLOAD_SAMPLE_RATE_ALWAYS = 1;
  public static final int UPLOAD_SAMPLE_RATE_NEVER = 0;
}
