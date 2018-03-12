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

import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.profilo.ipc.TraceContext;
import java.io.File;
import javax.annotation.concurrent.GuardedBy;

public final class PerfEventsProvider implements TraceOrchestrator.TraceProvider {

  public static final int PROVIDER_MAJOR_FAULTS = ProvidersRegistry.newProvider("major_faults");
  public static final int PROVIDER_THREAD_SCHEDULE =
      ProvidersRegistry.newProvider("thread_schedule");

  @GuardedBy("this")
  private PerfEventsSession mSession = null;

  @Override
  public synchronized void onEnable(TraceContext context, File extraDataFolder) {
    PerfEventsSession session = mSession;
    if (session == null) {
      session = new PerfEventsSession();
      mSession = session;
    }

    if (session.attach(context.enabledProviders)) {
      session.start();
    }
  }

  @Override
  public synchronized void onDisable(TraceContext context, File extraDataFolder) {
    PerfEventsSession session = mSession;
    if (session != null) {
      session.stop();
      session.detach();
    }
  }
}
