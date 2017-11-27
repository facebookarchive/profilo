// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.logger;

public interface LoggerCallbacks {

  void onLoggerException(Throwable t);
}
