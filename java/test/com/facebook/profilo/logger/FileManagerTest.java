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

import static org.assertj.core.api.Java6Assertions.assertThat;
import static org.junit.Assert.fail;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.when;

import android.content.Context;
import com.google.common.base.Function;
import com.google.common.collect.FluentIterable;
import com.google.common.io.Files;
import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.util.concurrent.TimeUnit;
import javax.annotation.Nullable;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class FileManagerTest {

  private File mFolder;
  private FileManager mFileManager;
  private File mTempFile;
  private Context mContext;

  @Before
  public void setup() throws IOException {
    mFolder = Files.createTempDir();
    mContext = mock(Context.class);
    mTempFile = File.createTempFile("fake-trace-", FileManager.TMP_SUFFIX, mFolder);
    Files.touch(mTempFile);

    when(mContext.getFilesDir()).thenReturn(mFolder);
    mFileManager = new FileManager(mContext, mFolder);
    mFileManager.setTrimThreshold(Integer.MAX_VALUE);
    // Age out after a day
    mFileManager.setMaxScheduledAge(TimeUnit.DAYS.toSeconds(1));
  }

  @After
  public void teardown() {
    deleteDir(mFolder);
  }

  @Test
  public void testFileManagerInitWithCustomFolder() {
    File customFolder = Files.createTempDir();
    FileManager fileManager = new FileManager(mContext, customFolder);
    assertThat(fileManager.getFolder().getAbsolutePath()).isEqualTo(customFolder.getAbsolutePath());
  }

  @Test
  public void testFileManagerInitWithFakeCustomFolder() {
    File customFolder = new File("/dev/null/void");
    FileManager fileManager = new FileManager(mContext, customFolder);
    assertThat(fileManager.getFolder().getParentFile().getAbsolutePath())
        .isEqualTo(mFolder.getAbsolutePath());
  }

  @Test
  public void testNormalFileManagerInitFromFilesFolder() {
    FileManager fileManager = new FileManager(mContext, null);
    assertThat(fileManager.getFolder().getParentFile().getAbsolutePath())
        .isEqualTo(mFolder.getAbsolutePath());
  }

  @Test
  public void testDeleteAllRemovesTmpFiles() throws Exception {
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesAlsoCrashDumpFiles() throws Exception {
    File dumpFolder = mFileManager.getCrashDumpFolder();
    dumpFolder.mkdirs();
    File dumpFile = File.createTempFile("test-dump", "dump", mFolder);
    Files.touch(dumpFile);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesConfiguration() throws Exception {
    File confiFile = new File(mFolder, "ProfiloInitFileConfig.json");
    assertThat(confiFile.createNewFile()).isTrue();
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesMmapBuffers() throws Exception {
    File mmapFolder = mFileManager.getMmapBufferFolder();
    if (!mmapFolder.exists()) {
      mmapFolder.mkdirs();
    }
    File.createTempFile("mmap_buffer", "buff", mFileManager.getMmapBufferFolder());
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mmapFolder);
  }

  @Test
  public void testDeleteAllRemovesTrimmableFilesScheduledForUpload() throws Exception {
    mFileManager.addFileToUploads(mTempFile, false);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesUntrimmableFilesScheduledForUpload() throws Exception {
    mFileManager.addFileToUploads(mTempFile, true);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesTrimmableFilesAlreadyUploaded() throws Exception {
    mFileManager.addFileToUploads(mTempFile, false);
    for (File f : mFileManager.getDefaultFilesToUpload()) {
      mFileManager.handleSuccessfulUpload(f);
    }

    assertThat(getFilesInFolderNoDirs(mFolder)).isNotEmpty(); // kept the trace after upload
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testDeleteAllRemovesUntrimmableFilesAlreadyUploaded() throws Exception {
    mFileManager.addFileToUploads(mTempFile, true);
    for (File f : mFileManager.getPriorityFilesToUpload()) {
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
    assertThat(uploadDir.createNewFile()).overridingErrorMessage("Failed to setup test").isTrue();

    mFileManager.addFileToUploads(mTempFile, false);
    assertThat(mFileManager.mFileManagerStatistics.errorsCreatingUploadDir).isEqualTo(1);

    assertThat(uploadDir.delete()).overridingErrorMessage("Failed to cleanup after test").isTrue();

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testErrorsMoveUpdatesCorrectly() throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.errorsMove).isEqualTo(0);
    // Delete the file before trying to add it to uploads -> cause move failure.
    assertThat(mTempFile.delete())
        .overridingErrorMessage("Unable to delete file to setup test")
        .isTrue();
    mFileManager.addFileToUploads(mTempFile, false);
    assertThat(mFileManager.mFileManagerStatistics.errorsMove).isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testAddedFilesToUploadUpdatesCorrectly() throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.getAddedFilesToUpload()).isEqualTo(0);
    mFileManager.addFileToUploads(mTempFile, false);
    assertThat(mFileManager.mFileManagerStatistics.getAddedFilesToUpload()).isEqualTo(1);

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
    for (int i = 0; i < numTracesLimit + 1; i++) {
      File f = File.createTempFile("fake-trace-", FileManager.TMP_SUFFIX, mFolder);
      Files.touch(f);
      // Add both trimmable and untrimmable files.
      mFileManager.addFileToUploads(f, i % 2 != 0);
    }

    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToCount).isEqualTo(0);
    // Upload all the pending files
    for (File f : mFileManager.getDefaultFilesToUpload()) {
      mFileManager.handleSuccessfulUpload(f);
    }
    for (File f : mFileManager.getPriorityFilesToUpload()) {
      mFileManager.handleSuccessfulUpload(f);
    }
    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToCount).isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  private void testTrimmedDueToAgeUpdatesCorrectly(boolean priority) throws Exception {
    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToAge).isEqualTo(0);

    mTempFile.setLastModified(0); // last modified in 1970 :-)
    mFileManager.addFileToUploads(mTempFile, priority);

    // Adding this old file will trigger a trim due to age
    assertThat(mFileManager.mFileManagerStatistics.trimmedDueToAge).isEqualTo(1);

    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  @Test
  public void testTrimmedDueToAgeUpdatesCorrectlyForDefaultTrace() throws Exception {
    testTrimmedDueToAgeUpdatesCorrectly(false);
  }

  @Test
  public void testTrimmedDueToAgeUpdatesCorrectlyForPriorityTrace() throws Exception {
    testTrimmedDueToAgeUpdatesCorrectly(true);
  }

  @Test
  public void testErrorsTrimmingUpdatesCorrectly() throws Exception {
    mFileManager.addFileToUploads(mTempFile, false);
    for (File f : mFileManager.getDefaultFilesToUpload()) {
      f.setLastModified(0); // last modified in 1970
    }

    // Create a destination folder with same name so move fails
    String filename = mTempFile.getName();
    File other =
        new File(
            mFolder, filename.substring(0, filename.lastIndexOf('.')) + FileManager.LOG_SUFFIX);
    assertThat(other.mkdir()).overridingErrorMessage("Failed to create directory").isTrue();
    assertThat(mFileManager.mFileManagerStatistics.errorsTrimming).isEqualTo(0);
    mFileManager.getDefaultFilesToUpload(); // trigger an age-based trim
    assertThat(mFileManager.mFileManagerStatistics.errorsTrimming).isEqualTo(1);
    deleteDir(other);
    mFileManager.deleteAllFiles();
    assertNoFilesInFolder(mFolder);
  }

  private void assertNoFilesInFolder(final File folder) {
    assertThat(folder).exists();
    assertThat(folder).isDirectory();

    FluentIterable.from(Files.fileTraverser().breadthFirst(folder))
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

  // Deletes a File that we made "undeletable" above
  private static void deleteDir(File f) {
    // so directories are empty when we get to them
    FluentIterable.from(Files.fileTraverser().depthFirstPostOrder(f))
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
