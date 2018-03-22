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

import static org.assertj.core.api.Java6Assertions.assertThat;
import static org.junit.Assert.fail;

import com.google.common.base.Function;
import com.google.common.io.Files;
import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

public class FileManagerTest {

  private File mFolder;
  private FileManager mFileManager;
  private File mTempFile;

  @Before
  public void setup() throws IOException {
    mFolder = Files.createTempDir();
    mTempFile = File.createTempFile("fake-trace-", FileManager.TMP_SUFFIX, mFolder);
    Files.touch(mTempFile);

    mFileManager = new FileManager(mFolder);
    mFileManager.setTrimThreshold(Integer.MAX_VALUE);
    // Age out after a day
    mFileManager.setMaxScheduledAge(TimeUnit.DAYS.toSeconds(1));
  }

  @After
  public void teardown() {
    deleteDir(mFolder);
  }

  @Test
  public void testDeleteAllRemovesTmpFiles() throws Exception {
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesTrimmableFilesScheduledForUpload() throws Exception {
    mFileManager.addFileToUploads(mTempFile, true);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesUntrimmableFilesScheduledForUpload() throws Exception {
    mFileManager.addFileToUploads(mTempFile, false);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesTrimmableFilesAlreadyUploaded() throws Exception {
    mFileManager.addFileToUploads(mTempFile, true);
    for (File f: mFileManager.getTrimmableFilesToUpload()) {
      mFileManager.handleSuccessfulUpload(f);
    }

    assertThat(getFilesInFolderNoDirs(mFolder)).isNotEmpty(); // kept the trace after upload
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesUntrimmableFilesAlreadyUploaded() throws Exception {
    mFileManager.addFileToUploads(mTempFile, false);
    for (File f: mFileManager.getUntrimmableFilesToUpload()) {
      mFileManager.handleSuccessfulUpload(f);
    }
    assertThat(getFilesInFolderNoDirs(mFolder)).isNotEmpty();

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testErrorsCreatingUploadDirUpdatesCorrectly() throws Exception {
    // create a file titled upload. Prevents creating upload dir.
    File uploadDir = new File(mFolder, FileManager.UPLOAD_FOLDER);
    assertThat(uploadDir.createNewFile())
        .overridingErrorMessage("Failed to setup test").isTrue();

    mFileManager.addFileToUploads(mTempFile, true);
    assertThat(mFileManager.mFileManagerStatistics.errorsCreatingUploadDir)
        .isEqualTo(1);

    assertThat(uploadDir.delete())
        .overridingErrorMessage("Failed to cleanup after test").isTrue();

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testErrorsDeleteUpdatesCorrectly() throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.errorsDelete)
        .isEqualTo(0);
    mFileManager.addFileToUploads(mTempFile, true);

    // Make files undeletable by converting to a non-empty dir of same name
    for (File f: mFileManager.getAllFiles()) {
      makeUndeletable(f);
    }
    mFileManager.deleteAllFiles();
    assertThat(mFileManager.mFileManagerStatistics.errorsDelete)
        .isEqualTo(1);

    for (File f: mFileManager.getAllFiles()) {
      deleteDir(f);
    }

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testErrorsMoveUpdatesCorrectly() throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.errorsMove).isEqualTo(0);
    // Delete the file before trying to add it to uploads -> cause move failure.
    assertThat(mTempFile.delete())
        .overridingErrorMessage("Unable to delete file to setup test").isTrue();
    mFileManager.addFileToUploads(mTempFile, true);
    assertThat(mFileManager.mFileManagerStatistics.errorsMove).isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testAddedFilesToUploadUpdatesCorrectly() throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.getAddedFilesToUpload())
        .isEqualTo(0);
    mFileManager.addFileToUploads(mTempFile, true);
    assertThat(mFileManager.mFileManagerStatistics.getAddedFilesToUpload())
        .isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testTrimmedDueToCountUpdatesCorrectly() throws Exception {
    int numTracesLimit = 5;
    // start clean
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
    mFileManager.setTrimThreshold(numTracesLimit);

    // Add a bunch of files to be uploaded (limit + 1)
    for (int i = 0; i< numTracesLimit + 1; i++) {
      File f = File.createTempFile("fake-trace-", FileManager.TMP_SUFFIX, mFolder);
      Files.touch(f);
      mFileManager.addFileToUploads(f, true);
    }

    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToCount).isEqualTo(0);
    // Upload all the pending files
    for (File f: mFileManager.getTrimmableFilesToUpload()) {
      mFileManager.handleSuccessfulUpload(f);
    }
    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToCount).isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testTrimmedDueToAgeUpdatesCorrectly() throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToAge).isEqualTo(0);

    mTempFile.setLastModified(0); // last modified in 1970 :-)
    mFileManager.addFileToUploads(mTempFile, true);

    // Adding this old file will trigger a trim due to age
    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToAge).isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testErrorsTrimmingUpdatesCorrectly() throws Exception {
    mFileManager.addFileToUploads(mTempFile, true);
    for (File f: mFileManager.getTrimmableFilesToUpload()) {
      f.setLastModified(0); // last modified in 1970
    }

    // Create a destination folder with same name so move fails
    String filename = mTempFile.getName();
    File other = new File(mFolder,
        filename.substring(0, filename.lastIndexOf('.')) + FileManager.LOG_SUFFIX);
    assertThat(other.mkdir()).overridingErrorMessage("Failed to create directory").isTrue();
    assertThat(mFileManager.mFileManagerStatistics.errorsTrimming).isEqualTo(0);
    mFileManager.getTrimmableFilesToUpload(); // trigger an age-based trim
    assertThat(mFileManager.mFileManagerStatistics.errorsTrimming).isEqualTo(1);
    deleteDir(other);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  private void assertNoFilesInFolder(final File folder) {
    assertThat(folder).exists();
    assertThat(folder).isDirectory();

    Files.fileTreeTraverser()
        .breadthFirstTraversal(folder)
        .transform(
            new Function<File, Void>() {
              @Nullable
              @Override
              public Void apply(@Nullable File input) {
                if (input != null && input.exists() && input.isFile()) {
                  fail("Folder " + folder.getPath() + " contains " + input.getPath());
                }
                return null;
              }
            });
  }

  private File[] getFilesInFolderNoDirs(final File folder) {
    assertThat(folder).exists();
    assertThat(folder).isDirectory();

    return folder.listFiles(
        new FileFilter() {
          @Override
          public boolean accept(File pathname) {
            return pathname.isFile();
          }
        });
  }

  // Make a file undeletable by making it a non-empty directory.
  private static void makeUndeletable(File f) throws IOException {
    if (!f.delete()) {
      return; // Nothing to do
    }
    f.mkdir();
    File.createTempFile("foo", null, f);
  }

  // Deletes a File that we made "undeletable" above
  private static void deleteDir(File f) {
    Files.fileTreeTraverser()
        .postOrderTraversal(f) // so directories are empty when we get to them
        .transform(
            new Function<File, Void>() {

              @Nullable
              @Override
              public Void apply(@Nullable File input) {
                if (input == null) {
                  return null;
                }
                input.deleteOnExit(); // just in case
                input.delete(); // doesn't matter, the VM will try to clean up after us anyway
                return null;
              }
            });

  }
}
