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

package com.facebook.profilo.provider.stacktrace;

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
    SoLoader.loadLibrary("profilo_stacktrace");
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
          case "7.1.2":
            result = nativeCheck(CPUProfiler.TRACER_ART_UNWINDC_7_1_2);
            break;
          case "7.1.1":
            result = nativeCheck(CPUProfiler.TRACER_ART_UNWINDC_7_1_1);
            break;
          case "7.1":
          case "7.1.0":
            result = nativeCheck(CPUProfiler.TRACER_ART_UNWINDC_7_1_0);
            break;
          case "7.0":
          case "7.0.0":
            result = nativeCheck(CPUProfiler.TRACER_ART_UNWINDC_7_0_0);
            break;
          case "6.0":
          case "6.0.1":
            result = nativeCheck(CPUProfiler.TRACER_ART_UNWINDC_6_0);
            break;
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
    return new File(context.getFilesDir(), "ProfiloArtUnwindcCompat_" + Build.VERSION.RELEASE);
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

  @DoNotStrip
  private static native boolean nativeCheck(int tracer);
}
