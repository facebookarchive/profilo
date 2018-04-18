// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.provider.mappingdensity;

import android.util.Log;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.core.TraceEvents;
import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import javax.annotation.Nullable;

@DoNotStrip
public class MappingDensityProvider implements TraceOrchestrator.TraceProvider {

  static {
    SoLoader.loadLibrary("profilo_mappingdensity");
  }

  public static final int PROVIDER_MAPPINGDENSITY = ProvidersRegistry.newProvider("mapping_density");

  @Nullable private TraceContext mSavedContext;

  @Override
  public synchronized void onEnable(TraceContext context, File extraDataFolder) {
    if (!TraceEvents.isEnabled(PROVIDER_MAPPINGDENSITY) || mSavedContext != null) {
      return;
    }
    mSavedContext = context;
    doDump(extraDataFolder, "start");
  }

  /** @see TraceOrchestrator.TraceProvider#onDisable */
  @Override
  public synchronized void onDisable(TraceContext context, File extraDataFolder) {
    if (mSavedContext == null || mSavedContext.traceId != context.traceId) {
      return; // Ensuring that the context which started the provider will stop it
    }
    mSavedContext = null;
    doDump(extraDataFolder, "end");
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
