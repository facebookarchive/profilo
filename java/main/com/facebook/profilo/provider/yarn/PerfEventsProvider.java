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

package com.facebook.profilo.provider.yarn;

import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import javax.annotation.concurrent.GuardedBy;

public final class PerfEventsProvider extends BaseTraceProvider {

  public static final String PROVIDER_MAJOR_FAULTS_NAME = "major_faults";
  public static final String PROVIDER_THREAD_SCHEDULE_NAME = "thread_schedule";

  public static final int PROVIDER_MAJOR_FAULTS =
      ProvidersRegistry.newProvider(PROVIDER_MAJOR_FAULTS_NAME);
  public static final int PROVIDER_THREAD_SCHEDULE =
      ProvidersRegistry.newProvider(PROVIDER_THREAD_SCHEDULE_NAME);

  @GuardedBy("this")
  private PerfEventsSession mSession = null;

  private boolean mEnabled;

  public PerfEventsProvider() {
    super("profilo_yarn");
  }

  @Override
  protected void enable() {
    PerfEventsSession session = mSession;
    if (session == null) {
      session = new PerfEventsSession();
      mSession = session;
    }

    if (session.attach(getEnablingTraceContext().enabledProviders)) {
      mEnabled = true;
      session.start();
    }
  }

  @Override
  protected void disable() {
    mEnabled = false;
    PerfEventsSession session = mSession;
    if (session != null) {
      session.stop();
      session.detach();
    }
  }

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_MAJOR_FAULTS | PROVIDER_THREAD_SCHEDULE;
  }

  @Override
  protected int getTracingProviders() {
    if (!mEnabled || getEnablingTraceContext() == null) {
      return 0;
    }
    return getEnablingTraceContext().enabledProviders & getSupportedProviders();
  }
}
