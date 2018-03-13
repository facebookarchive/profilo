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

package com.facebook.profilo.core;

import com.facebook.profilo.ipc.TraceContext;
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
