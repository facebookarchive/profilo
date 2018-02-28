// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.config;

public interface SystemControlConfig {

  public long getUploadMaxBytes();

  public long getUploadBytesPerUpdate();

  public long getUploadTimePeriodSec();
}
