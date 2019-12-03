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
import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.io.IOException;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;
import javax.annotation.Nullable;

@DoNotStrip
public class MmapBufferManager {

  public static final String BUFFER_FILE_SUFFIX = ".buff";

  static {
    SoLoader.loadLibrary("profilo_mmapbuf");
  }

  @DoNotStrip private final HybridData mHybridData;
  private volatile @Nullable String mMmapFileName;
  private final File mFolder;
  private final Context mContext;
  private final long mConfigId;
  private AtomicBoolean mAllocated;
  private AtomicBoolean mEnabled;

  @DoNotStrip
  private static native HybridData initHybrid();

  public MmapBufferManager(long configId, File folder, Context context) {
    mConfigId = configId;
    mFolder = folder;
    mContext = context;
    mAllocated = new AtomicBoolean(false);
    mEnabled = new AtomicBoolean(false);
    mHybridData = initHybrid();
  }

  public @Nullable String getCurrentMmapFilename() {
    return mMmapFileName;
  }

  public boolean isEnabled() {
    return mEnabled.get();
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

  public boolean allocateBuffer(int size) {
    File folder = mFolder;
    if (folder == null) {
      throw new IllegalStateException("Mmap folder is not set");
    }
    if (!mAllocated.compareAndSet(false, true)) {
      return false;
    }

    if (!folder.exists() && !folder.mkdirs()) {
      return false;
    }
    String fileName = UUID.randomUUID().toString() + BUFFER_FILE_SUFFIX;
    String mmapBufferPath = null;
    try {
      mmapBufferPath = folder.getCanonicalPath() + File.separator + fileName;
    } catch (IOException ignored) {
      return false;
    }
    mMmapFileName = fileName;
    boolean res = nativeAllocateBuffer(size, mmapBufferPath, getVersionCode(), mConfigId);
    mEnabled.set(res);
    return res;
  }

  @DoNotStrip
  private native boolean nativeAllocateBuffer(int size, String path, int buildId, long configId);

  @DoNotStrip
  public native void nativeUpdateHeader(int controllers, int providers, int trigger);

  /**
   * De-allocates current memory mapped buffer and deletes the buffer file. This operation is unsafe
   * and currently serve merely as stub for future dynamic buffer management extensions. All tracing
   * should be disabled before this method can be called.
   */
  @DoNotStrip
  public native void nativeDeallocateBuffer();
}
