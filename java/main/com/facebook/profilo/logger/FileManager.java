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

package com.facebook.profilo.logger;

import android.content.Context;
import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import javax.annotation.Nullable;

/**
 * The FileManager stores the traces under a given base folder. In that folder the FileManager
 * creates a subfolder {@link #UPLOAD_FOLDER} where traces that are to be uploaded are
 * stored. When they are uploaded or when they are discarded from the upload queue, they are moved
 * back to the base folder. To distinguish traces that are in progress from archived traces,
 * the files corresponding to ongoing traces have a ".tmp" extension/ (Filename is currently set in
 * {@link Trace}. Should be moved here...)
 *
 * Files can be added as either "trimmable" or "untrimmable". Untrimmable files are not subject
 * to any automatic file trimming/folder cleanup.
 *
 * Both the upload folder and the base folder are trimmed. Currently this is done when files are
 * added to the folders, but that may be more intelligent some day... The upload folder is trimmed
 * based on the age of the trace {@link #mMaxScheduledTracesAgeMillis}, and the base folder is
 * trimmed based on the number of traces in the folder {@link #mMaxArchivedTraces}.
 * The ".tmp" files are currently ignored by the trimming algorithms.
 */
public class FileManager {

  public static class FileManagerStatistics {
    int errorsDelete;
    int errorsMove;
    int errorsCreatingUploadDir;
    int errorsTrimming;
    int trimmedDueToCount;
    int trimmedDueToAge;
    int addedFilesToUpload;

    public int getTotalErrors() {
      return errorsDelete + errorsMove + errorsCreatingUploadDir + errorsTrimming;
    }

    public int getTrimmedDueToCount() {
      return trimmedDueToCount;
    }

    public int getTrimmedDueToAge() {
      return trimmedDueToAge;
    }

    public int getAddedFilesToUpload() {
      return addedFilesToUpload;
    }
  }

  private static final String TAG = "FileManager";

  static final String UPLOAD_FOLDER = "upload";
  static final String LOG_SUFFIX = ".log";
  static final String ZIP_SUFFIX = ".zip";
  public static final String TMP_SUFFIX = ".tmp";
  public static final String UNTRIMMABLE_PREFIX = "override-";
  private int mMaxArchivedTraces = 0;
  private long mMaxScheduledTracesAgeMillis = 0;

  // Visible for testing
  FileManagerStatistics mFileManagerStatistics =
      new FileManagerStatistics();

  public static final FilenameFilter TRIMMABLE_FILES_FILTER =
      new FilenameFilter() {
        @Override
        public boolean accept(File dir, String filename) {
          return !filename.startsWith(UNTRIMMABLE_PREFIX)
              && (filename.endsWith(LOG_SUFFIX)
                  || filename.endsWith(ZIP_SUFFIX)
                  || filename.endsWith(TMP_SUFFIX));
        }
      };

  public static final FilenameFilter UNTRIMMABLE_FILES_FILTER =
      new FilenameFilter() {
        @Override
        public boolean accept(File dir, String filename) {
          return filename.startsWith(UNTRIMMABLE_PREFIX) && filename.endsWith(LOG_SUFFIX);
        }
      };

  private final File mBaseFolder;

  public FileManager(File folder) {
    mBaseFolder = folder;
  }

  public FileManager(Context context) {
    this(getBaseFolder(context));
  }

  private static File getBaseFolder(Context context) {
    File internalCacheDir = context.getCacheDir();
    File internalDataDir = context.getFilesDir();

    if (internalCacheDir != null &&
        (internalCacheDir.exists() || internalCacheDir.mkdirs())) {
      return internalCacheDir;
    }

    return internalDataDir;
  }

  public void setTrimThreshold(int numTraces) {
    mMaxArchivedTraces = numTraces;
  }

  public void setMaxScheduledAge(long maxScheduledTracesAgeSec) {
    mMaxScheduledTracesAgeMillis = maxScheduledTracesAgeSec * 1000L;
  }

  /**
   * Schedules a file for upload.
   *
   * @param file file to schedule for upload
   * @param trimmable true if the file should be subject to automatic cleanup
   */
  public void addFileToUploads(File file, boolean trimmable) {
    String filename = file.getName();
    int index = filename.lastIndexOf('.');
    if (index != -1) {
      filename = filename.substring(0, index);
    }
    filename += LOG_SUFFIX;
    if (!trimmable) {
      filename = UNTRIMMABLE_PREFIX + filename;
    }

    File uploadFolder = getUploadFolder();

    if (uploadFolder.isDirectory() || uploadFolder.mkdirs()) {
      File destination = new File(uploadFolder, filename);
      if (file.renameTo(destination)) {
        mFileManagerStatistics.addedFilesToUpload++;
      } else {
        mFileManagerStatistics.errorsMove++;
      }

      trimFolderByAge(uploadFolder, mBaseFolder, mMaxScheduledTracesAgeMillis);
      trimFolderByFileCount(mBaseFolder, mMaxArchivedTraces);
    } else {
      mFileManagerStatistics.errorsCreatingUploadDir++;
    }
  }

  public void handleSuccessfulUpload(File file) {
    File newPath = new File(mBaseFolder, file.getName());
    if (moveOrDelete(file, newPath)) {
      trimFolderByFileCount(mBaseFolder, mMaxArchivedTraces);
    }
  }

  public List<File> getTrimmableFilesToUpload() {
    File uploadFolder = getUploadFolder();
    trimFolderByAge(uploadFolder, mBaseFolder, mMaxScheduledTracesAgeMillis);

    List<File> files = getFiles(uploadFolder, TRIMMABLE_FILES_FILTER);
    // Order by name == order by date due to naming convention
    Collections.sort(files, new Comparator<File>() {
          @Override
          public int compare(File lhs, File rhs) {
            return lhs.getName().compareTo(rhs.getName());
          }
        });

    return files;
  }

  public List<File> getUntrimmableFilesToUpload() {
    List<File> files = getFiles(getUploadFolder(), UNTRIMMABLE_FILES_FILTER);

    // Order by name == order by date due to naming convention
    Collections.sort(files, new Comparator<File>() {
          @Override
          public int compare(File lhs, File rhs) {
            return lhs.getName().compareTo(rhs.getName());
          }
        });

    return files;
  }

  public boolean deleteAllFiles() {
    boolean success = true;
    for (File file: getAllFiles()) {
      if (file.exists() && !file.delete()) {
          success = false;
          mFileManagerStatistics.errorsDelete++;
      }
    }
    return success;
  }

  public Iterable<File> getAllFiles() {
    List<File> allFiles = new ArrayList<>();
    allFiles.addAll(getFiles(getUploadFolder(), UNTRIMMABLE_FILES_FILTER));
    allFiles.addAll(getFiles(getUploadFolder(), TRIMMABLE_FILES_FILTER));
    allFiles.addAll(getFiles(getFolder(), UNTRIMMABLE_FILES_FILTER));
    allFiles.addAll(getFiles(getFolder(), TRIMMABLE_FILES_FILTER));
    return allFiles;
  }

  /**
   *   Tries to move the source to destination, and tries to delete source if moving fails.
   */
  private boolean moveOrDelete(File source, @Nullable File destination) {
    if (destination != null) {
      if (source.renameTo(destination)) {
        return true;
      }
      mFileManagerStatistics.errorsMove++;
    }

    if (source.exists() && !source.delete()) {
      mFileManagerStatistics.errorsDelete++;
    }

    return false;
  }

  public File getFolder() {
    return mBaseFolder;
  }

  private File getUploadFolder() {
    return new File(mBaseFolder, UPLOAD_FOLDER);
  }

  private void trimFolderByFileCount(
      File folder,
      int maxSize) {

    if (!folder.exists() && !folder.isDirectory()) {
      return;
    }

    List<File> files = getFiles(folder, TRIMMABLE_FILES_FILTER);

    if (files.size() > maxSize) {
      // Order by name == order by date due to naming convention
      Collections.sort(files, new Comparator<File>() {
            @Override
            public int compare(File lhs, File rhs) {
              return lhs.getName().compareTo(rhs.getName());
            }
          });
      for (File file : files.subList(0, files.size() - maxSize)) {
        if (file.delete()) {
          mFileManagerStatistics.trimmedDueToCount++;
        } else {
          mFileManagerStatistics.errorsTrimming++;
        }
      }
    }
  }

  private void trimFolderByAge(
      File source,
      File destination,
      long maxAgeMs) {

    if (!source.exists() && !source.isDirectory()) {
      return;
    }

    long minTime = System.currentTimeMillis() - maxAgeMs;

    for (File file : getFiles(source, TRIMMABLE_FILES_FILTER)) {
      if (file.lastModified() < minTime) {
        if (moveOrDelete(file, new File(destination, file.getName()))) {
          mFileManagerStatistics.trimmedDueToAge++;
        } else {
          mFileManagerStatistics.errorsTrimming++;
        }

      }
    }
  }

  private List<File> getFiles(File folder, FilenameFilter filter) {
    File[] files = folder.listFiles(filter);
    if (files == null) {
      return Collections.EMPTY_LIST;
    }

    return Arrays.asList(files);
  }

  public FileManagerStatistics getAndResetStatistics() {
    FileManagerStatistics fStats = mFileManagerStatistics;
    mFileManagerStatistics = new FileManagerStatistics();
    return fStats;
  }
}
