// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.core;

import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.logger.Logger;
import java.util.Set;

class CoreTraceAnnotationsListener extends DefaultTraceOrchestratorListener {

  private static void addTraceAnnotation(int key, String stringKey, String stringValue) {
    Logger.writeEntryWithString(
        ProfiloConstants.PROVIDER_PROFILO_SYSTEM,
        EntryType.TRACE_ANNOTATION,
        key,
        ProfiloConstants.NO_MATCH,
        0,
        stringKey,
        stringValue);
  }

  @Override
  public void onProvidersInitialized() {
    // Writing a marker block to denote providers init finish
    int pushId =
        Logger.writeEntryWithoutMatch(
            ProfiloConstants.PROVIDER_PROFILO_SYSTEM, EntryType.MARK_PUSH, 0, 0);
    Logger.writeEntry(
        ProfiloConstants.PROVIDER_PROFILO_SYSTEM,
        EntryType.STRING_NAME,
        pushId,
        "Profilo.ProvidersInitialized");
    Logger.writeEntryWithoutMatch(ProfiloConstants.PROVIDER_PROFILO_SYSTEM, EntryType.MARK_POP, 0);
  }

  @Override
  public void onProvidersStop(int activeProviders) {
    Set<String> activeProviderNames =
        ProvidersRegistry.getRegisteredProvidersByBitMask(activeProviders);
    StringBuilder activeProvidersStr = new StringBuilder();
    for (String p : activeProviderNames) {
      if (activeProvidersStr.length() != 0) {
        activeProvidersStr.append(",");
      }
      activeProvidersStr.append(p);
    }

    addTraceAnnotation(
        Identifiers.ACTIVE_PROVIDERS, "Active providers", activeProvidersStr.toString());
  }
}
