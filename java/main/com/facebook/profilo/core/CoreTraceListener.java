/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.facebook.profilo.core;

import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

class CoreTraceListener extends DefaultTraceOrchestratorListener {

  private static void addTraceAnnotation(int key, String stringKey, String stringValue) {

    int returnedMatchID =
        Logger.writeStandardEntry(
            ProfiloConstants.NONE,
            Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.TRACE_ANNOTATION,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            key,
            ProfiloConstants.NO_MATCH,
            ProfiloConstants.NONE);
    if (stringKey != null) {
      returnedMatchID =
          Logger.writeBytesEntry(
              ProfiloConstants.NONE,
              Logger.SKIP_PROVIDER_CHECK,
              EntryType.STRING_KEY,
              returnedMatchID,
              stringKey);
    }
    Logger.writeBytesEntry(
        ProfiloConstants.NONE,
        Logger.SKIP_PROVIDER_CHECK,
        EntryType.STRING_VALUE,
        returnedMatchID,
        stringValue);
  }

  @Override
  public void onProvidersInitialized() {
    // Writing a marker block to denote providers init finish
    int pushId =
        Logger.writeStandardEntry(
            ProfiloConstants.NONE,
            Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.MARK_PUSH,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            0,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE);
    Logger.writeBytesEntry(
        ProfiloConstants.NONE,
        Logger.SKIP_PROVIDER_CHECK,
        EntryType.STRING_NAME,
        pushId,
        "Profilo.ProvidersInitialized");
    Logger.writeStandardEntry(
        ProfiloConstants.NONE,
        Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
        EntryType.MARK_POP,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        0,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE);
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

  @Override
  public void onTraceStart(TraceContext context) {
    if (TraceEvents.isProviderNamesInitialized()) {
      return;
    }
    // Initialize provider names
    List<String> registeredProviders = ProvidersRegistry.getRegisteredProviders();
    HashMap<String, Integer> providerNamesMap = new HashMap<>(registeredProviders.size());
    for (String providerName : registeredProviders) {
      providerNamesMap.put(providerName, ProvidersRegistry.getBitMaskFor(providerName));
    }
    TraceEvents.initProviderNames(providerNamesMap);
  }
}
