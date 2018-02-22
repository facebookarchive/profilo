// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.yarn;

import com.facebook.loom.core.ProvidersRegistry;
import com.facebook.loom.core.TraceOrchestrator;
import com.facebook.loom.ipc.TraceContext;
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
