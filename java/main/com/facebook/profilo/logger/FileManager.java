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
package com.facebook.profilo.logger;

import android.content.Context;
import java.io.File;
import java.io.FileFilter;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import javax.annotation.Nullable;

/**
 * The FileManager stores the traces under a given base folder. In that folder the FileManager
 * creates a subfolder {@link #UPLOAD_FOLDER} where traces that are to be uploaded are stored. When
 * they are uploaded or when they are discarded from the upload queue, they are moved back to the
 * base folder. To distinguish traces that are in progress from archived traces, the files
 * corresponding to ongoing traces have a ".tmp" extension/ (Filename is currently set in {@link
 * Trace}. Should be moved here...)
 *
 * <p>Files can be added as either "trimmable" or "untrimmable". Untrimmable files are not subject
 * to any automatic file trimming/folder cleanup.
 *
 * <p>Both the upload folder and the base folder are trimmed. Currently this is done when files are
 * added to the folders, but that may be more intelligent some day... The upload folder is trimmed
 * based on the age of the trace {@link #mMaxScheduledTracesAgeMillis}, and the base folder is
 * trimmed based on the number of traces in the folder {@link #mMaxArchivedTraces}. The ".tmp" files
 * are currently ignored by the trimming algorithms.
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

  static final String PROFILO_FOLDER = "profilo";
  static final String UPLOAD_FOLDER = "upload";
  static final String CRASH_DUMPS_FOLDER = "crash_dumps";
  static final String MMAP_BUFFER_FOLDER = "mmap_buffer";
  static final String LOG_SUFFIX = ".log";
  static final String ZIP_SUFFIX = ".zip";
  public static final String TMP_SUFFIX = ".tmp";
  public static final String PRIORITY_TRACE_PREFIX = "override-";
  private int mMaxArchivedTraces = 0;
  private long mMaxScheduledTracesAgeMillis = 0;

  // Visible for testing
  FileManagerStatistics mFileManagerStatistics = new FileManagerStatistics();

  public static final FilenameFilter DEFAULT_FILES_FILTER =
      new FilenameFilter() {
        @Override
        public boolean accept(File dir, String filename) {
          return !filename.startsWith(PRIORITY_TRACE_PREFIX)
              && (filename.endsWith(LOG_SUFFIX)
                  || filename.endsWith(ZIP_SUFFIX)
                  || filename.endsWith(TMP_SUFFIX));
        }
      };

  public static final FilenameFilter PRIORITY_FILES_FILTER =
      new FilenameFilter() {
        @Override
        public boolean accept(File dir, String filename) {
          return filename.startsWith(PRIORITY_TRACE_PREFIX) && filename.endsWith(LOG_SUFFIX);
        }
      };

  private final File mBaseFolder;
  private File mUploadFolder;
  private File mCrashDumpFolder;
  private File mMmapBufferFolder;

  public FileManager(Context context, @Nullable File customFolder) {
    // Default location ("files/profilo") or user-supplied location
    if (customFolder != null && (customFolder.exists() || customFolder.mkdirs())) {
      mBaseFolder = customFolder;
    } else {
      // If unable to create the custom folder or not provided, fallback to default
      mBaseFolder = new File(context.getFilesDir(), PROFILO_FOLDER);
      if (!mBaseFolder.exists() && !mBaseFolder.mkdirs()) {
        throw new IllegalStateException("Unable to initialize Profilo folder");
      }
    }
    mUploadFolder = new File(mBaseFolder, UPLOAD_FOLDER);
    mCrashDumpFolder = new File(mBaseFolder, CRASH_DUMPS_FOLDER);
    mMmapBufferFolder = new File(mBaseFolder, MMAP_BUFFER_FOLDER);
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
   * @param priority true if the file should be prioritized to upload first and with quota exempts
   */
  public void addFileToUploads(File file, boolean priority) {
    String filename = file.getName();
    int index = filename.lastIndexOf('.');
    if (index != -1) {
      filename = filename.substring(0, index);
    }
    filename += LOG_SUFFIX;
    if (priority) {
      filename = PRIORITY_TRACE_PREFIX + filename;
    }

    File uploadFolder = getUploadFolder();

    if (uploadFolder.isDirectory() || uploadFolder.mkdirs()) {
      File destination = new File(uploadFolder, filename);
      if (file.renameTo(destination)) {
        mFileManagerStatistics.addedFilesToUpload++;
      } else {
        mFileManagerStatistics.errorsMove++;
      }

      trimFolderByAge(
          uploadFolder,
          mBaseFolder,
          mMaxScheduledTracesAgeMillis,
          DEFAULT_FILES_FILTER,
          PRIORITY_FILES_FILTER);
      trimFolderByFileCount(
          mBaseFolder, mMaxArchivedTraces, DEFAULT_FILES_FILTER, PRIORITY_FILES_FILTER);
    } else {
      mFileManagerStatistics.errorsCreatingUploadDir++;
    }
  }

  public void handleSuccessfulUpload(File file) {
    File newPath = new File(mBaseFolder, file.getName());
    if (moveOrDelete(file, newPath)) {
      trimFolderByFileCount(
          mBaseFolder, mMaxArchivedTraces, DEFAULT_FILES_FILTER, PRIORITY_FILES_FILTER);
    }
  }

  public List<File> getDefaultFilesToUpload() {
    File uploadFolder = getUploadFolder();
    trimFolderByAge(uploadFolder, mBaseFolder, mMaxScheduledTracesAgeMillis, DEFAULT_FILES_FILTER);

    List<File> files = getFiles(uploadFolder, DEFAULT_FILES_FILTER);
    // Order by name == order by date due to naming convention
    Collections.sort(
        files,
        new Comparator<File>() {
          @Override
          public int compare(File lhs, File rhs) {
            return lhs.getName().compareTo(rhs.getName());
          }
        });

    return files;
  }

  public List<File> getPriorityFilesToUpload() {
    File uploadFolder = getUploadFolder();
    trimFolderByAge(uploadFolder, mBaseFolder, mMaxScheduledTracesAgeMillis, PRIORITY_FILES_FILTER);

    List<File> files = getFiles(uploadFolder, PRIORITY_FILES_FILTER);
    // Order by name == order by date due to naming convention
    Collections.sort(
        files,
        new Comparator<File>() {
          @Override
          public int compare(File lhs, File rhs) {
            return lhs.getName().compareTo(rhs.getName());
          }
        });

    return files;
  }

  public boolean deleteAllFiles() {
    boolean success = true;
    for (File file : getAllFiles()) {
      if (file.exists() && !file.delete()) {
        success = false;
        mFileManagerStatistics.errorsDelete++;
      }
    }
    return success;
  }

  public Iterable<File> getAllFiles() {
    List<File> allFiles = new ArrayList<>();
    allFiles.addAll(getAllFiles(getFolder()));
    allFiles.addAll(getAllFiles(getUploadFolder()));
    allFiles.addAll(getAllFiles(getCrashDumpFolder()));
    allFiles.addAll(getAllFiles(getMmapBufferFolder()));
    return allFiles;
  }

  /** Tries to move the source to destination, and tries to delete source if moving fails. */
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

  public File getUploadFolder() {
    return mUploadFolder;
  }

  public File getCrashDumpFolder() {
    return mCrashDumpFolder;
  }

  public File getMmapBufferFolder() {
    return mMmapBufferFolder;
  }

  private void trimFolderByFileCount(File folder, int maxSize, FilenameFilter... filters) {
    if (filters.length == 0) {
      return;
    }
    if (!folder.exists() && !folder.isDirectory()) {
      return;
    }

    ArrayList<File> files = new ArrayList<>();
    for (FilenameFilter filter : filters) {
      files.addAll(getFiles(folder, filter));
    }

    if (files.size() > maxSize) {
      // Order by name == order by date due to naming convention
      Collections.sort(
          files,
          new Comparator<File>() {
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
      File source, File destination, long maxAgeMs, FilenameFilter... filters) {
    if (filters.length == 0) {
      return;
    }
    if (!source.exists() && !source.isDirectory()) {
      return;
    }

    ArrayList<File> files = new ArrayList<>();
    for (FilenameFilter filter : filters) {
      files.addAll(getFiles(source, filter));
    }

    long minTime = System.currentTimeMillis() - maxAgeMs;
    for (File file : files) {
      if (file.lastModified() < minTime) {
        if (moveOrDelete(file, new File(destination, file.getName()))) {
          mFileManagerStatistics.trimmedDueToAge++;
        } else {
          mFileManagerStatistics.errorsTrimming++;
        }
      }
    }
  }

  private List<File> getAllFiles(File folder) {
    File[] files =
        folder.listFiles(
            new FileFilter() {
              @Override
              public boolean accept(File file) {
                return file.isFile();
              }
            });
    if (files == null) {
      return Collections.EMPTY_LIST;
    }

    return Arrays.asList(files);
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

  @Nullable
  public static File getMmapBufferFolderIfExists(Context context) {
    File baseFolder = new File(context.getFilesDir(), PROFILO_FOLDER);
    File mmapFilesFolder = new File(baseFolder, MMAP_BUFFER_FOLDER);
    if (!mmapFilesFolder.exists()) {
      return null;
    }
    return mmapFilesFolder;
  }
}
