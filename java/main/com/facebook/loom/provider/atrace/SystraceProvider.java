// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.atrace;

import com.facebook.loom.core.BaseTraceProvider;
import com.facebook.loom.core.LoomConstants;

public final class SystraceProvider extends BaseTraceProvider {

  @Override
  protected void enable() {
    Atrace.enableSystrace();
  }

  @Override
  protected void disable() {
    Atrace.restoreSystrace();
  }

  @Override
  protected int getSupportedProviders() {
    return LoomConstants.PROVIDER_OTHER;
  }
}
