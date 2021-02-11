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
package com.facebook.profilo.provider.mappings;

import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.logger.MultiBufferLogger;

public final class MemoryMappingsProvider extends BaseTraceProvider {

  public static final int PROVIDER_MAPPINGS = ProvidersRegistry.newProvider("memory_mappings");

  public MemoryMappingsProvider() {
    super("profilo_mappings");
  }

  @Override
  protected void enable() {}

  @Override
  protected void disable() {
    nativeLogMappings(getLogger());
  }

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_MAPPINGS;
  }

  private static native void nativeLogMappings(MultiBufferLogger logger);

  @Override
  protected int getTracingProviders() {
    return PROVIDER_MAPPINGS;
  }
}
