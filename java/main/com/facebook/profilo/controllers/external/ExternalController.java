// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.controllers.external;

import com.facebook.profilo.config.ControllerConfig;
import com.facebook.profilo.core.TraceController;
import javax.annotation.Nullable;

public class ExternalController implements TraceController {

  public static class Config {

    public int providers;
    public int cpuSamplingRateMs;
  }

  @Override
  public int evaluateConfig(@Nullable Object context, ControllerConfig ignored) {
    return getConfigFromContext(context).providers;
  }

  @Override
  public int getCpuSamplingRateMs(Object context, ControllerConfig ignored) {
    return getConfigFromContext(context).cpuSamplingRateMs;
  }

  private Config getConfigFromContext(Object context) {
    if (!(context instanceof Config)) {
      throw new RuntimeException(
          "Context for ExternalController must " + "be an instance of ExternalController.Config");
    }
    return (Config) context;
  }

  @Override
  public boolean contextsEqual(int fstInt, @Nullable Object fst, int sndInt, @Nullable Object snd) {
    // Always honor stop requests.

    return true;
  }
}
