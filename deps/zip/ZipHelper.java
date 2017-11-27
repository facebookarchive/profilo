// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.file.zip;

import android.util.Log;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URI;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public class ZipHelper {

  private static final String TAG = ZipHelper.class.getSimpleName();

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
    try {
      zipDirectory(directory, zippedFile);
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

  private static void zipDirectory(File directory, File outFile) throws IOException {
    try (BufferedOutputStream outputStream =
            new BufferedOutputStream(new FileOutputStream(outFile), BUFFER_OUTPUT_STREAM);
        ZipOutputStream outZipStream = new ZipOutputStream(outputStream); ) {
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
    try {
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
    } catch (Exception e) {
      Log.e(TAG, "failed to delete directory", e);
    }
  }
}
