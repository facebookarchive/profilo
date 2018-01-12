// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.processmetadata;

import com.facebook.loom.core.TraceOrchestrator;
import com.facebook.loom.ipc.TraceContext;
import com.facebook.soloader.SoLoader;
import java.io.File;

public class ProcessMetadataProvider implements TraceOrchestrator.TraceProvider {

  static {
    SoLoader.loadLibrary("loom_processmetadata");
  }

  @Override
  public void onEnable(TraceContext context, File extraDataFolder) {
    nativeLogProcessMetadata();
  }

  @Override
  public void onDisable(TraceContext context, File extraDataFolder) {
    nativeLogProcessMetadata();
  }

  private static native void nativeLogProcessMetadata();
}
