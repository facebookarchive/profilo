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
package com.facebook.profilo.mmapbuf.core;

import android.util.Log;
import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.util.UUID;
import javax.annotation.Nullable;

@DoNotStrip
public class Buffer {

  private static final String LOG_TAG = "Prflo/Buffer";

  static {
    SoLoader.loadLibrary("profilo_mmapbuf");
  }

  @DoNotStrip private final HybridData mHybridData;

  private Buffer(HybridData data) {
    mHybridData = data;
  }

  private File getBufferContainingFolder() {
    File bufferFilePath = new File(getFilePath());
    return bufferFilePath.getParentFile();
  }

  public boolean isFileBacked() {
    return getFilePath() != null;
  }

  @Nullable
  public synchronized String generateMemoryMappingFilePath() {
    if (!isFileBacked()) {
      return null;
    }
    String filePath = getMemoryMappingFilePath();
    if (filePath == null) {
      MmapBufferFileHelper helper = new MmapBufferFileHelper(getBufferContainingFolder());
      String fileName = MmapBufferFileHelper.getMemoryMappingFilename(UUID.randomUUID().toString());
      filePath = helper.ensureFilePath(fileName);
      if (filePath != null) {
        updateMemoryMappingFilePath(filePath);
      }
    }

    return filePath;
  }

  public synchronized void updateId(String id) {
    if (!isFileBacked()) {
      return;
    }
    String fileName = MmapBufferFileHelper.getBufferFilename(id);
    MmapBufferFileHelper helper = new MmapBufferFileHelper(getBufferContainingFolder());
    String filePath = helper.ensureFilePath(fileName);
    if (filePath == null) {
      return;
    }

    try {
      nativeUpdateId(id);
      updateFilePath(filePath);
    } catch (Exception ex) {
      Log.e(LOG_TAG, "Id update failed", ex);
    }
  }

  @DoNotStrip
  public synchronized native void updateHeader(
      int providers, long longContext, long traceId, long configId);

  @DoNotStrip
  private native void nativeUpdateId(String sessionId);

  @DoNotStrip
  public synchronized native void updateFilePath(String filePath);

  @DoNotStrip
  public synchronized native void updateMemoryMappingFilePath(String mappingFilePath);

  @DoNotStrip
  public synchronized native String getMemoryMappingFilePath();

  @DoNotStrip
  public synchronized native String getFilePath();
}
