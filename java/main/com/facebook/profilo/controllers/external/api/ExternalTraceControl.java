// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.controllers.external.api;

import com.facebook.profilo.controllers.external.ExternalController;
import com.facebook.profilo.core.LoomConstants;
import com.facebook.profilo.core.TraceControl;
import com.facebook.profilo.logger.Trace;

public class ExternalTraceControl {

  public static boolean startTrace(int providers, int cpuSamplingRateMs) {
    TraceControl control = TraceControl.get();
    if (control == null) {
      return false;
    }

    ExternalController.Config config = new ExternalController.Config();
    config.providers = providers;
    config.cpuSamplingRateMs = cpuSamplingRateMs;

    //
    // We set FLAG_MANUAL because external control implies no file manager and upload constraint
    // configuration. Therefore, it doesn't make sense to throttle things based on the fallback
    // config in DefaultConfigProvider.
    //
    final int flags = Trace.FLAG_MANUAL;
    return control.startTrace(LoomConstants.TRIGGER_EXTERNAL, flags, config, 0);
  }

  public static void stopTrace() {
    TraceControl control = TraceControl.get();
    if (control == null) {
      return;
    }
    control.stopTrace(LoomConstants.TRIGGER_EXTERNAL, null, 0);
  }
}
