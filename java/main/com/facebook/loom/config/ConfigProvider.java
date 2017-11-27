// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.config;

import javax.annotation.Nullable;

public interface ConfigProvider {
  interface ConfigUpdateListener {
    void onConfigUpdated(Config config);
  }

  void setConfigUpdateListener(@Nullable ConfigUpdateListener listener);
  Config getFullConfig();
}
