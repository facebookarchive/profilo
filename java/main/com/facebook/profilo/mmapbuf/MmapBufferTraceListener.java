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
package com.facebook.profilo.mmapbuf;

import com.facebook.profilo.core.DefaultTraceOrchestratorListener;
import com.facebook.profilo.core.TraceControl;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Trace;
import java.util.List;

public class MmapBufferTraceListener extends DefaultTraceOrchestratorListener {

  private final MmapBufferManager mMmapBufferManager;

  public MmapBufferTraceListener(MmapBufferManager manager) {
    mMmapBufferManager = manager;
  }

  private void updateTracingState() {
    TraceControl tc = TraceControl.get();
    if (tc == null) {
      return;
    }
    List<TraceContext> currentTraces = tc.getCurrentTraces();
    int longContext = 0;
    int providers = 0;
    long inMemoryTraceId = 0;
    for (TraceContext ctx : currentTraces) {
      providers |= ctx.enabledProviders;
      if ((ctx.flags & Trace.FLAG_MEMORY_ONLY) != 0) {
        longContext = (int) ctx.longContext;
        inMemoryTraceId = ctx.traceId;
        break;
      }
    }
    mMmapBufferManager.nativeUpdateHeader(providers, longContext, inMemoryTraceId);
  }

  @Override
  public void onTraceStart(TraceContext context) {
    updateTracingState();
  }

  @Override
  public void onTraceStop(TraceContext context) {
    updateTracingState();
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    updateTracingState();
  }
}
