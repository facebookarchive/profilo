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

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.List;
import javax.annotation.Nullable;

public class MmapBufferFileHelper {

  public interface FileDeletionBlacklist {
    List<String> getFilenames();
  }

  public static final String BUFFER_FILE_SUFFIX = ".buff";
  public static final String MEMORY_MAPPING_FILE_SUFFIX = ".maps";

  private final File mMmapFilesFolder;
  public static final Object DUMP_FILES_LOCK = new Object();

  public MmapBufferFileHelper(File mmapFilesFolder) {
    mMmapFilesFolder = mmapFilesFolder;
  }

  @Nullable
  public File findBufferFileById(String id) {
    File mmapFolder = getFolderIfExists();
    if (mmapFolder == null) {
      return null;
    }
    final String targetFilename = getBufferFilename(id);
    File[] foundFiles =
        mmapFolder.listFiles(
            new FilenameFilter() {
              @Override
              public boolean accept(File dir, String name) {
                return targetFilename.equals(name);
              }
            });
    if (foundFiles == null || foundFiles.length != 1) {
      return null;
    }
    return foundFiles[0];
  }

  public void deleteOldBufferFiles(@Nullable FileDeletionBlacklist blacklist) throws IOException {
    File mmapFolder = getFolderIfExists();
    if (mmapFolder == null) {
      return;
    }
    File[] mmapFiles = mmapFolder.listFiles();
    if (mmapFiles == null) {
      return;
    }
    List<String> blacklistFilenames = blacklist.getFilenames();
    for (File file : mmapFiles) {
      if (blacklistFilenames.contains(file.getCanonicalPath())) {
        continue;
      }
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
    return sanitizeFilename(id) + BUFFER_FILE_SUFFIX;
  }

  public static String getMemoryMappingFilename(String id) {
    return sanitizeFilename(id) + MEMORY_MAPPING_FILE_SUFFIX;
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
