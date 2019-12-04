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

import static com.facebook.profilo.mmapbuf.MmapBufferManager.BUFFER_FILE_SUFFIX;

import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.profilo.logger.FileManager;
import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import javax.annotation.Nullable;

class MmapBufferFileHelper {

  private static final int MAX_DUMPS_TO_PROCESS = 5;

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

  @Nullable private File mMmapFilesFolder;
  @Nullable private FileManager mFileManager;

  MmapBufferFileHelper() {}

  /** Get current pending buffer files to process */
  public List<File> getBufferFilesToProcess() {
    File mmapFolder = getFolder();
    if (mmapFolder == null) {
      return Collections.emptyList();
    }
    ArrayList<File> filesToProcess = new ArrayList<>();
    File[] mmapFolderFiles = mmapFolder.listFiles();
    if (mmapFolderFiles == null) {
      return Collections.emptyList();
    }
    for (File nextFile : mmapFolderFiles) {
      if (!nextFile.getName().endsWith(BUFFER_FILE_SUFFIX) || nextFile.length() == 0) {
        if (!nextFile.delete()) {
          nextFile.deleteOnExit();
        }
        continue;
      }
      filesToProcess.add(nextFile);
    }
    // Cleanup old files if they happen to be not processed yet.
    if (filesToProcess.size() > MAX_DUMPS_TO_PROCESS) {
      Collections.sort(filesToProcess, MOST_RECENT_FILES_COMPARATOR);
      while (filesToProcess.size() > MAX_DUMPS_TO_PROCESS) {
        int lastIdx = filesToProcess.size() - 1;
        File file = filesToProcess.get(lastIdx);
        if (!file.delete()) {
          file.deleteOnExit();
        }
        filesToProcess.remove(lastIdx);
      }
    }
    return filesToProcess;
  }

  void ensureFileManager() {
    if (mFileManager != null) {
      return;
    }
    TraceOrchestrator tc = TraceOrchestrator.get();
    if (tc == null) {
      return;
    }
    mFileManager = tc.getFileManager();
  }

  @Nullable
  private File getFolder() {
    if (mMmapFilesFolder != null) {
      return mMmapFilesFolder;
    }
    ensureFileManager();
    if (mFileManager == null) {
      return null;
    }
    File mmapFolder = mFileManager.getMmapBufferFolder();
    if (mmapFolder.isDirectory() && mmapFolder.exists()) {
      mMmapFilesFolder = mmapFolder;
      return mmapFolder;
    }
    return null;
  }
}
