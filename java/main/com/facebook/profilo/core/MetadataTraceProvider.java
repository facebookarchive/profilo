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

import com.facebook.profilo.ipc.TraceContext;
import javax.annotation.Nullable;

/**
 * Base Class for Metadata providers which are always ON and log data at the start / end of a trace.
 * The only case when these providers are not logging is controller initiated trace abort: {@link
 * ProfiloConstants#ABORT_REASON_CONTROLLER_INITIATED}.
 */
public abstract class MetadataTraceProvider extends BaseTraceProvider {

  protected MetadataTraceProvider() {
    this(null);
  }

  protected MetadataTraceProvider(@Nullable String nativeLibrary) {
    super(nativeLibrary);
  }

  protected void logOnTraceStart(TraceContext context, ExtraDataFileProvider dataFileProvider) {}

  protected void logOnTraceEnd(TraceContext context, ExtraDataFileProvider dataFileProvider) {}

  @Override
  protected void onTraceStarted(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    logOnTraceStart(context, dataFileProvider);
  }

  @Override
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    if (context.abortReason == ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED) {
      return;
    }
    logOnTraceEnd(context, dataFileProvider);
  }

  @Override
  protected void enable() {}

  @Override
  protected void disable() {}

  @Override
  protected int getSupportedProviders() {
    return EVERY_PROVIDER_CHANGE;
  }

  @Override
  protected int getTracingProviders() {
    return 0;
  }
}
