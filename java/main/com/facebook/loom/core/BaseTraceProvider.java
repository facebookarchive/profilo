// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import com.facebook.loom.ipc.TraceContext;
import java.io.File;

public abstract class BaseTraceProvider implements TraceOrchestrator.TraceProvider {

  int savedProviders;

  @Override
  public void onEnable(TraceContext context, File extraDataFolder) {
    processStateChange();
  }

  @Override
  public void onDisable(TraceContext context, File extraDataFolder) {
    processStateChange();
  }

  private void processStateChange() {
    int providerMask = TraceEvents.enabledMask(getSupportedProviders());

    // Nothing changed - keep enabled
    if (savedProviders != 0 && TraceEvents.isEnabled(savedProviders)) {
      return;
    }
    // If provider was tracing stop first to reset the state
    if (savedProviders != 0) {
      disable();
    }
    // Current provider is on => enable
    if (providerMask != 0) {
      enable();
    }
    savedProviders = providerMask;
  }

  protected abstract void enable();

  protected abstract void disable();

  /** @return Supported provider mask */
  protected abstract int getSupportedProviders();
}
