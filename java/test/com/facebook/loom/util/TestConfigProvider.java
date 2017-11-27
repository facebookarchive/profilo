// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.util;

import com.facebook.loom.config.Config;
import com.facebook.loom.config.ConfigProvider;
import com.facebook.loom.config.ControllerConfig;
import com.facebook.loom.config.DefaultConfigProvider;
import com.facebook.loom.config.SystemControlConfig;
import java.lang.Override;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

public final class TestConfigProvider implements ConfigProvider {

  private static final int TEST_TRACE_TIMEOUT_MS = 30000;
  private final Set<Integer> mControllers = new HashSet<>();
  private ConfigUpdateListener mListener;
  private SystemControlConfig mSystemControlConfig =
    new DefaultConfigProvider().getFullConfig().getSystemControl();
  private int mTimedOutUploadSampleRate;

  public TestConfigProvider setControllers(Integer... controllers) {
    mControllers.clear();
    mControllers.addAll(Arrays.asList(controllers));
    return this;
  }

  @Override
  public void setConfigUpdateListener(ConfigUpdateListener listener) {
    mListener = listener;
  }

  public ConfigUpdateListener getConfigUpdateListener() {
    return mListener;
  }

  public void setSystemControlConfig(SystemControlConfig systemControlConfig) {
    mSystemControlConfig = systemControlConfig;
  }

  public void setTimedOutUploadSampleRate(int timedOutUploadSampleRate) {
    mTimedOutUploadSampleRate = timedOutUploadSampleRate;
  }

  @Override
  public Config getFullConfig() {
    return new Config() {
      @Override
      public RootControllerConfig getControllersConfig() {
        return new RootControllerConfig() {

          @Override
          public ControllerConfig getConfigForController(int controller) {
            return new ControllerConfig() {
            };
          }

          @Override
          public int getTraceTimeoutMs() {
            return TEST_TRACE_TIMEOUT_MS;
          }

          @Override
          public int getTimedOutUploadSampleRate() {
            return mTimedOutUploadSampleRate;
          }
        };
      }

      @Override
      public SystemControlConfig getSystemControl() {
        return mSystemControlConfig;
      }

      @Override
      public long getConfigID() {
        return 0xFACEB00C;
      }
    };
  }
}
