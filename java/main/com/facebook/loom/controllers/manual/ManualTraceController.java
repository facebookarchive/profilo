// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.controllers.manual;

import com.facebook.loom.config.ControllerConfig;
import com.facebook.loom.core.LoomConstants;
import com.facebook.loom.core.TraceController;

public class ManualTraceController implements TraceController {

  @Override
  public int evaluateConfig(Object context, ControllerConfig config) {
    return LoomConstants.PROVIDER_ASYNC
        | LoomConstants.PROVIDER_LIFECYCLE
        | LoomConstants.PROVIDER_QPL
        | LoomConstants.PROVIDER_OTHER
        | LoomConstants.PROVIDER_SYSTEM_COUNTERS
        | LoomConstants.PROVIDER_STACK_FRAME
        | LoomConstants.PROVIDER_LIGER
        | LoomConstants.PROVIDER_MAIN_THREAD_MESSAGES
        | LoomConstants.PROVIDER_LIGER_HTTP2
        | LoomConstants.PROVIDER_FBSYSTRACE
        | LoomConstants.PROVIDER_HIGH_FREQ_MAIN_THREAD_COUNTERS
        | LoomConstants.PROVIDER_TRANSIENT_NETWORK_DATA;
  }

  @Override
  public int getCpuSamplingRateMs(Object context, ControllerConfig config) {
    return 0;
  }

  @Override
  public boolean contextsEqual(int fstInt, Object fst, int sndInt, Object snd) {
    return fst == snd;
  }
}
