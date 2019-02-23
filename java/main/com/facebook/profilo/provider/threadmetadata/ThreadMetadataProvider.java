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

package com.facebook.profilo.provider.threadmetadata;

import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.ipc.TraceContext;

public final class ThreadMetadataProvider extends BaseTraceProvider {

  public ThreadMetadataProvider() {
    super("profilo_threadmetadata");
  }

  @Override
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    nativeLogThreadMetadata();
  }

  @Override
  protected void enable() {}

  @Override
  protected void disable() {}

  @Override
  protected int getSupportedProviders() {
    return EVERY_PROVIDER_CHANGE;
  }

  /* Log thread names and priorities. */
  private static native void nativeLogThreadMetadata();

  @Override
  protected int getTracingProviders() {
    return 0;
  }
}
