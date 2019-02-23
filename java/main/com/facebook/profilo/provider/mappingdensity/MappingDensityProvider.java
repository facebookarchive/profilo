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
package com.facebook.profilo.provider.mappingdensity;

import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.proguard.annotations.DoNotStrip;
import java.io.File;

@DoNotStrip
public final class MappingDensityProvider extends BaseTraceProvider {

  public static final int PROVIDER_MAPPINGDENSITY =
      ProvidersRegistry.newProvider("mapping_density");

  private boolean mEnabled;

  public MappingDensityProvider() {
    super("profilo_mappingdensity");
  }

  @Override
  protected void enable() {
    mEnabled = true;
  }

  @Override
  protected void disable() {
    mEnabled = false;
  }

  @Override
  protected void onTraceStarted(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    File extraDataFile = dataFileProvider.getExtraDataFile(context, this);
    if (mEnabled && extraDataFile != null) {
      dumpMappingDensities(
          "^/data/(data|app)",
          new File(extraDataFile.getParentFile(), extraDataFile.getName() + "-start").getPath());
    }
  }

  @Override
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    File extraDataFile = dataFileProvider.getExtraDataFile(context, this);
    if (mEnabled && extraDataFile != null) {
      dumpMappingDensities(
          "^/data/(data|app)",
          new File(extraDataFile.getParentFile(), extraDataFile.getName() + "-end").getPath());
    }
  }

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_MAPPINGDENSITY;
  }

  private static native void dumpMappingDensities(
      String mapRegex, String extraDataFile);

  @Override
  protected int getTracingProviders() {
    return mEnabled ? PROVIDER_MAPPINGDENSITY : 0;
  }
}
