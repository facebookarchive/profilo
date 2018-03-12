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

package com.facebook.profilo.provider.atrace;

import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;

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
