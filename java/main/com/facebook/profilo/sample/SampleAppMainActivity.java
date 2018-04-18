// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.sample;

import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.profilo.provider.atrace.SystraceProvider;
import com.facebook.profilo.provider.mappingdensity.MappingDensityProvider;
import com.facebook.profilo.provider.processmetadata.ProcessMetadataProvider;
import com.facebook.profilo.provider.stacktrace.StackFrameThread;
import com.facebook.profilo.provider.systemcounters.SystemCounterThread;
import com.facebook.profilo.provider.threadmetadata.ThreadMetadataProvider;
import com.facebook.profilo.provider.yarn.PerfEventsProvider;

public class SampleAppMainActivity extends BaseSampleAppMainActivity {
  @Override
  protected TraceOrchestrator.TraceProvider createProvider(String provider) {
    switch (provider) {
      case "atrace":
        return new SystraceProvider();
      case "mappingdensity":
        return new MappingDensityProvider();
      case "systemcounters":
        return new SystemCounterThread();
      case "stacktrace":
        return new StackFrameThread(getApplicationContext());
      case "threadmetadata":
        return new ThreadMetadataProvider();
      case "processmetadata":
        return new ProcessMetadataProvider(this.getApplicationContext());
      case "yarn":
        return new PerfEventsProvider();
      default:
        return super.createProvider(provider);
    }
  }
}
