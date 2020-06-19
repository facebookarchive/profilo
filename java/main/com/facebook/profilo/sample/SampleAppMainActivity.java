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
package com.facebook.profilo.sample;

import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.provider.atrace.SystraceProvider;
import com.facebook.profilo.provider.mappings.MemoryMappingsProvider;
import com.facebook.profilo.provider.perfevents.PerfEventsProvider;
import com.facebook.profilo.provider.processmetadata.ProcessMetadataProvider;
import com.facebook.profilo.provider.stacktrace.StackFrameThread;
import com.facebook.profilo.provider.systemcounters.SystemCounterThread;
import com.facebook.profilo.provider.threadmetadata.ThreadMetadataProvider;

public class SampleAppMainActivity extends BaseSampleAppMainActivity {
  @Override
  protected BaseTraceProvider createProvider(String provider) {
    switch (provider) {
      case "atrace":
        return new SystraceProvider();
      case "memorymappings":
        return new MemoryMappingsProvider();
      case "systemcounters":
        return new SystemCounterThread();
      case "stacktrace":
        return new StackFrameThread(getApplicationContext());
      case "threadmetadata":
        return new ThreadMetadataProvider();
      case "processmetadata":
        return new ProcessMetadataProvider(this.getApplicationContext());
      case "perfevents":
        return new PerfEventsProvider();
      default:
        return super.createProvider(provider);
    }
  }
}
