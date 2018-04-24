/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.facebook.profilo.util;

import com.facebook.profilo.config.Config;
import com.facebook.profilo.config.ConfigProvider;
import com.facebook.profilo.config.ControllerConfig;
import com.facebook.profilo.config.DefaultConfigProvider;
import com.facebook.profilo.config.SystemControlConfig;
import java.util.HashMap;

public final class TestConfigProvider implements ConfigProvider {

  private static final int TEST_TRACE_TIMEOUT_MS = 30000;
  private final HashMap<Integer, ControllerConfig> mControllers = new HashMap<>();
  private ConfigUpdateListener mListener;
  private SystemControlConfig mSystemControlConfig =
    new DefaultConfigProvider().getFullConfig().getSystemControl();
  private int mTimedOutUploadSampleRate;

  public TestConfigProvider setControllers(Integer... controllers) {
    mControllers.clear();
    for (int c : controllers) {
      mControllers.put(c, new ControllerConfig() {});
    }
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
            return mControllers.get(controller);
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
