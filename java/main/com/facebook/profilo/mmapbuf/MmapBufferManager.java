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
  private final MmapBufferFileHelper mFileHelper;
  private final Context mContext;
  private final long mConfigId;
  private AtomicBoolean mAllocated;
  private volatile @Nullable Buffer mBuffer;

  @DoNotStrip
  private static native HybridData initHybrid();

  public MmapBufferManager(long configId, File folder, Context context) {
    mConfigId = configId;
    mContext = context;
    mAllocated = new AtomicBoolean(false);
    mFileHelper = new MmapBufferFileHelper(folder);
    mHybridData = initHybrid();
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
  public Buffer allocateBuffer(int size, boolean filebacked) {
    if (!mAllocated.compareAndSet(false, true)) {
      return mBuffer;
    }

    if (filebacked) {
      String fileName = MmapBufferFileHelper.getBufferFilename(UUID.randomUUID().toString());
      String mmapBufferPath = mFileHelper.ensureFilePath(fileName);
      if (mmapBufferPath == null) {
        return null;
      }
      mBuffer = nativeAllocateBuffer(size, mmapBufferPath, getVersionCode(), mConfigId);
      return mBuffer;
    } else {
      mBuffer = nativeAllocateBuffer(size);
      return mBuffer;
    }
  }

  public synchronized boolean deallocateBuffer(Buffer buffer) {
    if (buffer != mBuffer) {
      return false;
    }
    return true; // cannot actually deallocate the buffer yet
  }

  @DoNotStrip
  @Nullable
  private native Buffer nativeAllocateBuffer(int size);

  @DoNotStrip
  @Nullable
  private native Buffer nativeAllocateBuffer(int size, String path, int buildId, long configId);

  @DoNotStrip
  private native boolean nativeDeallocateBuffer(Buffer buffer);
}
