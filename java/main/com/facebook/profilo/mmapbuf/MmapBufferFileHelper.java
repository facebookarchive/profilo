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

import java.io.File;
import java.io.IOException;
import java.util.Arrays;
import java.util.Comparator;
import javax.annotation.Nullable;

class MmapBufferFileHelper {

  public static final String BUFFER_FILE_SUFFIX = ".buff";

  private static final int MAX_DUMPS_TO_KEEP = 3;

  private static final Comparator<File> MOST_RECENT_FILES_COMPARATOR =
      new Comparator<File>() {
        @Override
        public int compare(File f1, File f2) {
          long f1LastModified = f1.lastModified();
          long f2LastModified = f2.lastModified();
          if (f1LastModified == f2LastModified) {
            return 0;
          }
          if (f1LastModified < f2LastModified) {
            return 1;
          } else {
            return -1;
          }
        }
      };

  private final File mMmapFilesFolder;
  public static final Object DUMP_FILES_LOCK = new Object();

  MmapBufferFileHelper(File mmapFilesFolder) {
    mMmapFilesFolder = mmapFilesFolder;
  }

  public void deleteOldBufferFiles() {
    File mmapFolder = getFolderIfExists();
    if (mmapFolder == null) {
      return;
    }
    File[] mmapFiles = mmapFolder.listFiles();
    if (mmapFiles == null) {
      return;
    }
    int filesCount = mmapFiles.length;
    if (filesCount <= MAX_DUMPS_TO_KEEP) {
      return;
    }
    Arrays.sort(mmapFiles, MOST_RECENT_FILES_COMPARATOR);
    for (int i = MAX_DUMPS_TO_KEEP; i < filesCount; i++) {
      File file = mmapFiles[i];
      synchronized (DUMP_FILES_LOCK) {
        if (file.exists()) {
          file.delete();
        }
      }
    }
  }

  @Nullable
  public String ensureFilePath(String fileName) {
    if (!mMmapFilesFolder.exists() && !mMmapFilesFolder.mkdirs()) {
      return null;
    }

    String mmapBufferPath = null;
    try {
      mmapBufferPath = mMmapFilesFolder.getCanonicalPath() + File.separator + fileName;
    } catch (IOException ignored) {
      // Ignored
    }

    return mmapBufferPath;
  }

  public static String getBufferFilename(String id) {
    return sanitizeFilename(id + BUFFER_FILE_SUFFIX);
  }

  public static String sanitizeFilename(String filename) {
    int len = filename.length();
    StringBuilder sanitizedFilename = new StringBuilder(len);
    for (int i = 0; i < len; i++) {
      char ch = filename.charAt(i);
      boolean isValid =
          (ch >= 'A' && ch <= 'Z')
              || (ch >= 'a' && ch <= 'z')
              || (ch >= '0' && ch <= '9')
              || ch == '-'
              || ch == '_'
              || ch == '.';
      sanitizedFilename.append(isValid ? ch : "_");
    }
    return sanitizedFilename.toString();
  }

  @Nullable
  private File getFolderIfExists() {
    if (mMmapFilesFolder.isDirectory() && mMmapFilesFolder.exists()) {
      return mMmapFilesFolder;
    }
    return null;
  }
}
