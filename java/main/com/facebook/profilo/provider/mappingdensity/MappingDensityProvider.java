// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.provider.mappingdensity;

import android.util.Log;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.proguard.annotations.DoNotStrip;
import java.io.File;

@DoNotStrip
public final class MappingDensityProvider extends BaseTraceProvider {

  public static final int PROVIDER_MAPPINGDENSITY = ProvidersRegistry.newProvider("mapping_density");

  public MappingDensityProvider() {
    super("profilo_mappingdensity");
  }

  @Override
  protected void enable() {}

  @Override
  protected void disable() {}

  @Override
  protected void onTraceStarted(TraceContext context, File extraDataFolder) {
    doDump(extraDataFolder, "start");
  }

  @Override
  protected void onTraceEnded(TraceContext context, File extraDataFolder) {
    doDump(extraDataFolder, "end");
  }

  @Override
  protected int getSupportedProviders() {
    return PROVIDER_MAPPINGDENSITY;
  }

  private static void doDump(File extraDataFolder, String dumpName) {
    extraDataFolder.mkdirs();
    if (extraDataFolder.exists()) {
      dumpMappingDensities("^/data/(data|app)", extraDataFolder.getPath(), dumpName);
    } else {
      Log.w("Profilo/MappingDensity", "nonexistent extras directory: " + extraDataFolder.getPath());
    }
  }

  private static native void dumpMappingDensities(
      String mapRegex, String extraDataFolderPath, String dumpName);
}
