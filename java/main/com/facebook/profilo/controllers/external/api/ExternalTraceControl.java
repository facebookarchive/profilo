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
package com.facebook.profilo.controllers.external.api;

import com.facebook.profilo.controllers.external.ExternalController;
import com.facebook.profilo.core.TraceControl;
import com.facebook.profilo.logger.Trace;

public class ExternalTraceControl {

  public static boolean startTrace(int providers, int cpuSamplingRateMs, int flags) {
    TraceControl control = TraceControl.get();
    if (control == null) {
      return false;
    }

    ExternalController.Config config = new ExternalController.Config();
    config.providers = providers;
    config.cpuSamplingRateMs = cpuSamplingRateMs;

    return control.startTrace(ExternalController.TRIGGER_EXTERNAL, flags, config, 0L);
  }

  public static boolean startTrace(int providers, int cpuSamplingRateMs) {
    //
    // We set FLAG_MANUAL because external control implies no file manager and upload constraint
    // configuration. Therefore, it doesn't make sense to throttle things based on the fallback
    // config in DefaultConfigProvider.
    //
    return startTrace(providers, cpuSamplingRateMs, Trace.FLAG_MANUAL);
  }

  public static void stopTrace() {
    TraceControl control = TraceControl.get();
    if (control == null) {
      return;
    }
    control.stopTrace(ExternalController.TRIGGER_EXTERNAL, null, 0L);
  }
}
