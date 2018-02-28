// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.threadmetadata;

import com.facebook.loom.core.TraceOrchestrator;
import com.facebook.loom.ipc.TraceContext;
import com.facebook.soloader.SoLoader;
import java.io.File;

public class ThreadMetadataProvider implements TraceOrchestrator.TraceProvider {

  static {
    SoLoader.loadLibrary("profilo_threadmetadata");
  }

  @Override
  public void onEnable(TraceContext context, File extraDataFolder) {}

  @Override
  public void onDisable(TraceContext context, File extraDataFolder) {
    // Always enabled, if this provider is installed.
    nativeLogThreadMetadata();
  }

  /* Log thread names and priorities. */
  private static native void nativeLogThreadMetadata();
}

