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

package com.facebook.profilo.config;

import javax.annotation.Nullable;

/**
 * This class implements a way to merge two {@link ConfigProvider} instances into a single instance.
 * This is achieved via layering - the top layer is queried first, the bottom layer is a
 * fallback. For values that represent sets, the union of both layers is returned.
 */
public class OverlayConfigProvider implements ConfigProvider, ConfigProvider.ConfigUpdateListener {

  private final ConfigProvider mTop;
  private final ConfigProvider mBottom;
  private Config mOverlayConfig;
  @Nullable private ConfigUpdateListener mListener;

  public OverlayConfigProvider(ConfigProvider top, ConfigProvider bottom) {
    mTop = top;
    mBottom = bottom;
    mTop.setConfigUpdateListener(this);
    mBottom.setConfigUpdateListener(this);
    mOverlayConfig = new OverlayConfig(mTop.getFullConfig(), mBottom.getFullConfig());
  }

  @Override
  public void setConfigUpdateListener(@Nullable ConfigUpdateListener listener) {
    mListener = listener;
  }

  @Override
  public Config getFullConfig() {
    return mOverlayConfig;
  }

  @Override
  public void onConfigUpdated(Config config) {
    mOverlayConfig = new OverlayConfig(mTop.getFullConfig(), mBottom.getFullConfig());
    ConfigUpdateListener listener = mListener;
    if (listener != null) {
      listener.onConfigUpdated(mOverlayConfig);
    }
  }

  private static class OverlayConfig implements Config {

    private final Config mTop;
    private final Config mBottom;
    private final OverlayRootControllerConfig mRootControllerConfig;
    private final OverlaySystemControlConfig mSystemControlConfig;

    OverlayConfig(Config top, Config bottom) {
      mTop = top;
      mBottom = bottom;
      mRootControllerConfig = new OverlayRootControllerConfig(
          mTop.getControllersConfig(),
          mBottom.getControllersConfig());
      mSystemControlConfig = new OverlaySystemControlConfig(
          mTop.getSystemControl(),
          mBottom.getSystemControl());
    }

    @Override
    public RootControllerConfig getControllersConfig() {
      return mRootControllerConfig;
    }

    @Override
    public SystemControlConfig getSystemControl() {
      return mSystemControlConfig;
    }

    @Override
    public long getConfigID() {
      return mTop.getConfigID() ^ mBottom.getConfigID();
    }
  }

  private static class OverlayRootControllerConfig implements Config.RootControllerConfig {

    private final Config.RootControllerConfig mTop;
    private final Config.RootControllerConfig mBottom;

    OverlayRootControllerConfig(
        Config.RootControllerConfig top,
        Config.RootControllerConfig bottom) {
      mTop = top;
      mBottom = bottom;
    }

    @Override
    public ControllerConfig getConfigForController(int controller) {
      ControllerConfig topConfig = mTop.getConfigForController(controller);
      if (topConfig != null) {
        return topConfig;
      }
      return mBottom.getConfigForController(controller);
    }

    @Override
    public int getTraceTimeoutMs() {
      return Math.max(mTop.getTraceTimeoutMs(), mBottom.getTraceTimeoutMs());
    }

    @Override
    public int getTimedOutUploadSampleRate() {
      if (mTop.getTimedOutUploadSampleRate() == 0) {
        return mBottom.getTimedOutUploadSampleRate();
      }
      if (mBottom.getTimedOutUploadSampleRate() == 0) {
        return mTop.getTimedOutUploadSampleRate();
      }
      return Math.min(mTop.getTimedOutUploadSampleRate(), mBottom.getTimedOutUploadSampleRate());
    }
  }

  private static class OverlaySystemControlConfig implements SystemControlConfig {

    private final SystemControlConfig mTop;
    private final SystemControlConfig mBottom;

    public OverlaySystemControlConfig(
        SystemControlConfig top,
        SystemControlConfig bottom) {
      mTop = top;
      mBottom = bottom;
    }

    public long getUploadMaxBytes() {
      return Math.max(mTop.getUploadMaxBytes(), mBottom.getUploadMaxBytes());
    }

    public long getUploadBytesPerUpdate() {
      return Math.max(mTop.getUploadBytesPerUpdate(), mBottom.getUploadBytesPerUpdate());
    }

    public long getUploadTimePeriodSec() {
      return Math.min(mTop.getUploadTimePeriodSec(), mBottom.getUploadTimePeriodSec());
    }
  }
}
