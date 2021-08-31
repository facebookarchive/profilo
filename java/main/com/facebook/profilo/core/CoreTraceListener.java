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
import com.facebook.profilo.logger.BufferLogger;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.mmapbuf.core.Buffer;
import java.util.Set;

class CoreTraceListener extends DefaultTraceOrchestratorListener {

  private static void addTraceAnnotation(
      Buffer buffer, int key, String stringKey, String stringValue) {

    int returnedMatchID =
        BufferLogger.writeStandardEntry(
            buffer,
            Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.TRACE_ANNOTATION,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            key,
            ProfiloConstants.NO_MATCH,
            ProfiloConstants.NONE);
    returnedMatchID =
        BufferLogger.writeBytesEntry(
            buffer, ProfiloConstants.NONE, EntryType.STRING_KEY, returnedMatchID, stringKey);
    BufferLogger.writeBytesEntry(
        buffer, ProfiloConstants.NONE, EntryType.STRING_VALUE, returnedMatchID, stringValue);
  }

  @Override
  public void onProvidersInitialized(TraceContext traceContext) {
    // Writing a marker block to denote providers init finish
    long time = System.nanoTime();
    int pushId =
        BufferLogger.writeStandardEntry(
            traceContext.mainBuffer,
            Logger.FILL_TID,
            EntryType.MARK_PUSH,
            time,
            ProfiloConstants.NONE,
            0,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE);
    BufferLogger.writeBytesEntry(
        traceContext.mainBuffer,
        ProfiloConstants.NONE,
        EntryType.STRING_NAME,
        pushId,
        "Profilo.ProvidersInitialized");
    BufferLogger.writeStandardEntry(
        traceContext.mainBuffer,
        Logger.FILL_TID,
        EntryType.MARK_POP,
        time,
        ProfiloConstants.NONE,
        0,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE);
  }

  @Override
  public void onProvidersStop(TraceContext traceContext, int activeProviders) {
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
        traceContext.mainBuffer,
        Identifiers.ACTIVE_PROVIDERS,
        "Active providers",
        activeProvidersStr.toString());
  }
}
