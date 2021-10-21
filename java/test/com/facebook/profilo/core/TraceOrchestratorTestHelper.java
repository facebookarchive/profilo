// (c) Facebook, Inc. and its affiliates. Confidential and proprietary.

package com.facebook.profilo.core;

import android.content.Context;
import android.util.SparseArray;
import com.facebook.profilo.config.ConfigProvider;

public class TraceOrchestratorTestHelper {

  public static TraceOrchestrator createTraceOrchestrator(
      Context context, ConfigProvider configProvider, BaseTraceProvider[] providers) {
    TraceOrchestrator to =
        new TraceOrchestrator(
            context,
            configProvider,
            providers,
            TraceOrchestrator.MAIN_PROCESS_NAME,
            true, // isMainProcess
            null); // Default trace location
    to.bind(context, new SparseArray<TraceController>(1));
    return to;
  }
}
