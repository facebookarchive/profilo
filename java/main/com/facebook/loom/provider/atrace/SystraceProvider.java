// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.atrace;

import com.facebook.loom.core.BaseTraceProvider;
import com.facebook.loom.core.ProvidersRegistry;

public final class SystraceProvider extends BaseTraceProvider {

  public static final int PROVIDER_ATRACE = ProvidersRegistry.newProvider("other");

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
    return PROVIDER_ATRACE;
  }
}
