// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.config;

import javax.annotation.Nullable;

public interface Config {

  RootControllerConfig getControllersConfig();

  SystemControlConfig getSystemControl();

  /**
   * @return a numeric identifier that represents this configuration
   */
  long getConfigID();

  interface RootControllerConfig {

    @Nullable ControllerConfig getConfigForController(int controller);

    /**
     * @return a maximum duration (timeout) for traces or -1 if none is set.
     * The amount is in milliseconds.
     */
    int getTraceTimeoutMs();

    /**
     * @return Sample rate at which to upload timed out traces. Value N means 1/N probability of
     * timed out trace to be uploaded. Default value is 0, which means that such traces should be
     * aborted.
     */
    int getTimedOutUploadSampleRate();
  }
}
