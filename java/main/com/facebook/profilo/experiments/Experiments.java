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

package com.facebook.profilo.experiments;

import android.text.TextUtils;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.logger.Logger;
import java.util.ArrayList;

public final class Experiments {

  public static volatile boolean STACKTRACE_ALTERNATIVE_TRACERS = false;

  public static void logActiveExperiments() {
    ArrayList<String> experiments = new ArrayList<>(1);
    if (STACKTRACE_ALTERNATIVE_TRACERS) {
      experiments.add("stacktrace_alternative_tracers");
    }

    if (!experiments.isEmpty()) {
      String experimentList = TextUtils.join(" ", experiments);

      Logger.writeEntryWithString(
          ProfiloConstants.PROVIDER_PROFILO_SYSTEM,
          EntryType.TRACE_ANNOTATION,
          0,
          ProfiloConstants.NO_MATCH,
          0,
          "Internal Tracing Experiments",
          experimentList);
    }
  }
}
