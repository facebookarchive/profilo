// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.provider.stacktrace;

import android.content.Context;
import android.os.Build;
import com.facebook.proguard.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

@DoNotStrip
public class ArtCompatibility {

  static {
    SoLoader.loadLibrary("loom_stacktrace");
  }

  private static final AtomicReference<Boolean> sIsCompatible = new AtomicReference<>(null);

  public static void clearFileCache(Context context) {
    File file = getFileForRelease(context);
    if (file.exists()) {
      if (!file.delete()) {
        file.deleteOnExit();
      }
    }
  }

  public static boolean isCompatible(Context context) {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
      return false;
    }

    Boolean cached = sIsCompatible.get();
    if (cached != null) {
      return cached;
    }

    try {
      File file = getFileForRelease(context);
      boolean result = false;

      if (file.exists()) {
        result = readCompatFile(file);
      } else {
        switch (Build.VERSION.RELEASE) {
          case "7.0":
            result = nativeCheckArt7_0();
            break;
          case "6.0":
          case "6.0.1":
            result = nativeCheckArt6_0();
            break;
          case "5.1":
          case "5.1.0":
          case "5.1.1":
            result = nativeCheckArt5_1();
            break;
          default:
            result = false;
        }
        writeCompatFile(file, result);
      }

      // Either a complete read or complete run + write. Safe to cache this value.
      if (sIsCompatible.compareAndSet(null, result)) {
        return result;
      } else {
        return sIsCompatible.get();
      }
    } catch (IOException e) {
      return false;
    }
  }

  private static File getFileForRelease(Context context) {
    return new File(context.getFilesDir(), "LoomArtProfiler_" + Build.VERSION.RELEASE);
  }

  private static boolean readCompatFile(File file) throws IOException {
    try (FileInputStream fis = new FileInputStream(file)) {
      return fis.read() == '1';
    }
  }

  private static void writeCompatFile(File file, boolean result) throws IOException {
    try (FileOutputStream fos = new FileOutputStream(file)) {
      fos.write(result ? '1' : '0');
    }
  }

  @DoNotStrip private static native boolean nativeCheckArt7_0();
  @DoNotStrip private static native boolean nativeCheckArt6_0();
  @DoNotStrip private static native boolean nativeCheckArt5_1();
}
