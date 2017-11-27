// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.config;

import com.facebook.loom.core.LoomConstants;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;

public class DefaultConfigProvider implements ConfigProvider {

  public static final int DEFAULT_TRACE_TIMEOUT_MS = 30000;
  public static final int DEFAULT_MAX_BYTES = 10000;
  public static final long DEFAULT_UPLOAD_PERIOD_SEC = TimeUnit.HOURS.toSeconds(1);
  public static final int DEFAULT_UPLOAD_BYTES_PER_UPDATE = 10000 / 24;

  public static final Config DEFAULT_CONFIG =
      new Config() {

        private final ControllerConfig mControllerConfig = new ControllerConfig() {};

        @Override
        public RootControllerConfig getControllersConfig() {
          return new RootControllerConfig() {

            @Override
            @Nullable
            public ControllerConfig getConfigForController(int controller) {
              if (controller == LoomConstants.TRIGGER_MANUAL
                  || controller == LoomConstants.TRIGGER_EXTERNAL) {
                return mControllerConfig;
              }
              return null;
            }

            @Override
            public int getTraceTimeoutMs() {
              return DEFAULT_TRACE_TIMEOUT_MS;
            }

            @Override
            public int getTimedOutUploadSampleRate() {
              return 0;
            }
          };
        }

        @Override
        public SystemControlConfig getSystemControl() {
          return new SystemControlConfig() {

            @Override
            public long getUploadMaxBytes() {
              return DEFAULT_MAX_BYTES;
            }

            @Override
            public long getUploadBytesPerUpdate() {
              return DEFAULT_UPLOAD_BYTES_PER_UPDATE;
            }

            @Override
            public long getUploadTimePeriodSec() {
              return DEFAULT_UPLOAD_PERIOD_SEC;
            }
          };
        }

        @Override
        public long getConfigID() {
          return 0;
        }
      };

  @Override
  public void setConfigUpdateListener(ConfigUpdateListener listener) {
  }

  @Override
  public Config getFullConfig() {
    return DEFAULT_CONFIG;
  }
}
