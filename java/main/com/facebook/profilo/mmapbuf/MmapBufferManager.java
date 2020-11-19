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
package com.facebook.profilo.mmapbuf;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Log;
import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;
import javax.annotation.Nullable;

@DoNotStrip
public class MmapBufferManager {

  private static final String LOG_TAG = "Profilo/MmapBufferMngr";

  static {
    SoLoader.loadLibrary("profilo_mmapbuf");
  }

  @DoNotStrip private final HybridData mHybridData;
  private volatile @Nullable String mMmapFileName;
  private @Nullable String mId;
  private volatile @Nullable File mMemoryMappingsFile;
  private final MmapBufferFileHelper mFileHelper;
  private final Context mContext;
  private final long mConfigId;
  private AtomicBoolean mAllocated;
  private @Nullable Buffer mBuffer;

  @DoNotStrip
  private static native HybridData initHybrid();

  public MmapBufferManager(long configId, File folder, Context context) {
    mConfigId = configId;
    mContext = context;
    mAllocated = new AtomicBoolean(false);
    mFileHelper = new MmapBufferFileHelper(folder);
    mHybridData = initHybrid();
  }

  public @Nullable String getCurrentMmapFilename() {
    return mMmapFileName;
  }

  public List<String> getCurrentFilenames() {
    if (mMmapFileName == null) {
      return Collections.emptyList();
    }
    ArrayList<String> filenames = new ArrayList<>(2);
    filenames.add(mMmapFileName);
    if (mMemoryMappingsFile != null) {
      filenames.add(mMemoryMappingsFile.getName());
    }
    return filenames;
  }

  public boolean isEnabled() {
    return mBuffer != null;
  }

  private int getVersionCode() {
    if (mContext == null) {
      return 0;
    }
    PackageManager pm = mContext.getPackageManager();
    if (pm == null) {
      return 0;
    }
    PackageInfo pi;
    try {
      pi = pm.getPackageInfo(mContext.getPackageName(), 0);
    } catch (PackageManager.NameNotFoundException | RuntimeException e) {
      return 0;
    }
    return pi.versionCode;
  }

  @Nullable
  public Buffer allocateBuffer(int size) {
    if (!mAllocated.compareAndSet(false, true)) {
      return mBuffer;
    }

    String fileName = MmapBufferFileHelper.getBufferFilename(UUID.randomUUID().toString());
    String mmapBufferPath = mFileHelper.ensureFilePath(fileName);
    if (mmapBufferPath == null) {
      return null;
    }
    mMmapFileName = fileName;

    mBuffer = nativeAllocateBuffer(size, mmapBufferPath, getVersionCode(), mConfigId);
    return mBuffer;
  }

  public synchronized void updateId(String id) {
    if (mBuffer == null) {
      return;
    }
    if (id.equals(mId)) {
      return;
    }

    String fileName = MmapBufferFileHelper.getBufferFilename(id);
    String filePath = mFileHelper.ensureFilePath(fileName);
    if (filePath == null) {
      return;
    }

    try {
      mBuffer.updateId(id);
      mBuffer.updateFilePath(filePath);
    } catch (Exception ex) {
      Log.e(LOG_TAG, "Id update failed", ex);
    }
    mId = id;
    mMmapFileName = fileName;
  }

  @Nullable
  public synchronized String generateMemoryMappingFilePath() {
    if (mBuffer == null) {
      return null;
    }
    if (mMemoryMappingsFile != null) {
      return mMemoryMappingsFile.getAbsolutePath();
    }

    String fileName = MmapBufferFileHelper.getMemoryMappingFilename(UUID.randomUUID().toString());
    String filePath = mFileHelper.ensureFilePath(fileName);
    if (filePath == null) {
      return null;
    }

    mBuffer.updateMemoryMappingFilename(fileName);

    mMemoryMappingsFile = new File(filePath);
    return filePath;
  }

  public synchronized void updateHeader(int providers, long longContext, long traceId) {
    if (mBuffer == null) {
      return;
    }
    mBuffer.updateHeader(providers, longContext, traceId);
  }

  @DoNotStrip
  @Nullable
  private native Buffer nativeAllocateBuffer(int size, String path, int buildId, long configId);

  /**
   * De-allocates current memory mapped buffer and deletes the buffer file. This operation is unsafe
   * and currently serve merely as stub for future dynamic buffer management extensions. All tracing
   * should be disabled before this method can be called.
   */
  @DoNotStrip
  public native void nativeDeallocateBuffer();
}
