// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

package com.facebook.file.zip;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

public class ZipHelper {
  public static final String ZIP_SUFFIX = ".zip";
  public static final String TMP_SUFFIX = ".tmp";

  private static final int BUFFER_SIZE_BYTES = 1024;
  private static final int BUFFER_OUTPUT_STREAM = 256 * 1024; // 256 kb

  public static boolean shouldZipDirectory(File directory) {
    return directory.isDirectory() && directory.list().length > 1;
  }

  /**
   * Takes a directory File and returns as compressed file of the directory
   *
   * @param directory directory that we would like to zip
   * @param suffix suffix of the output file
   * @return NULL if File is not a directory or there was an error during the compression
   */
  public static File getCompressedFile(File directory, String suffix) {
    if (!directory.isDirectory()) {
      return null;
    }
    String filename = directory.getName() + suffix;
    // zip the file
    File zippedFile = new File(directory.getParent(), filename);
    try (BufferedOutputStream bufferedOutputStream =
        new BufferedOutputStream(new FileOutputStream(zippedFile), BUFFER_OUTPUT_STREAM)) {
      zipDirectory(directory, bufferedOutputStream);
    } catch (IOException e) {
      // failed to zip file
      // delete and return
      zippedFile.delete();
      return null;
    }
    return zippedFile;
  }

  private static void addFileToZip(File base, String filePath, ZipOutputStream outZipStream)
      throws IOException {
    try (FileInputStream inputStream = new FileInputStream(new File(base, filePath)); ) {
      byte[] buffer = new byte[BUFFER_SIZE_BYTES];
      outZipStream.putNextEntry(new ZipEntry(filePath));
      int bytes_read;
      while ((bytes_read = inputStream.read(buffer)) > 0) {
        outZipStream.write(buffer, 0, bytes_read);
      }
    } finally {
      outZipStream.closeEntry();
    }
  }

  private static void addDirectoryToZip(File base, String directory, ZipOutputStream outZipStream)
      throws IOException {
    File fullPath = new File(base, directory).getAbsoluteFile();
    URI baseURI = base.toURI();

    String[] fileNames = fullPath.list();
    for (String fileName : fileNames) {
      File file = new File(fullPath, fileName);
      if (!file.exists()) {
        continue;
      }
      String relativePath = baseURI.relativize(file.toURI()).getPath();
      if (file.isFile()) {
        addFileToZip(base, relativePath, outZipStream);
      } else if (file.isDirectory()) {
        addDirectoryToZip(base, relativePath, outZipStream);
      }
    }
  }

  public static void zipDirectory(File directory, OutputStream outputStream) throws IOException {
    try (ZipOutputStream outZipStream = new ZipOutputStream(outputStream)) {
      addDirectoryToZip(directory, ".", outZipStream);
      outZipStream.flush();
      outZipStream.finish();
    }
  }

  /**
   * Helper function to delete directory and all the files in it
   *
   * @param directory
   */
  public static void deleteDirectory(File directory) {
    if (!directory.isDirectory()) {
      return;
    }
    String[] fileNames = directory.list();
    if (fileNames != null) {
      for (String fileName : fileNames) {
        File file = new File(directory, fileName);
        if (file.isDirectory()) {
          deleteDirectory(file);
        } else {
          file.delete();
        }
      }
    }
    directory.delete();
  }

  /**
   * Extracts the data from the InputStream onto the directory
   *
   * @param inputStream
   * @param extractDir
   * @return
   * @throws IOException
   */
  public static boolean extractZip(InputStream inputStream, File extractDir) throws IOException {
    ZipInputStream zis = new ZipInputStream(inputStream);
    ZipEntry entry = zis.getNextEntry();
    if (entry == null) {
      return false;
    }
    byte[] buffer = new byte[BUFFER_SIZE_BYTES];

    while (entry != null) {
      File newFile = new File(extractDir, entry.getName());
      if (!newFile.getCanonicalPath().startsWith(extractDir.getCanonicalPath())) {
        throw new IOException("Invalid entry name. Possible path traversal.");
      }
      if (entry.isDirectory()) {
        if (!newFile.isDirectory() && !newFile.mkdirs()) {
          throw new IOException("Can't create directory");
        }
      } else {
        File parent = newFile.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) {
          throw new IOException("Can't create parent directory");
        }
        // write file content
        try (FileOutputStream fos = new FileOutputStream(newFile)) {
          int len;
          while ((len = zis.read(buffer)) > 0) {
            fos.write(buffer, 0, len);
          }
        }
      }
      entry = zis.getNextEntry();
    }
    return true;
  }
}
