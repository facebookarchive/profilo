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
package com.facebook.profilo.core;

import java.io.File;
import java.util.List;

public interface BackgroundUploadService {

  int UPLOAD_FAILURE_REQUEST_FAILED = 1;
  int UPLOAD_FAILURE_NO_CONNECTION = 2;
  int UPLOAD_FAILURE_NO_BYTES_REMAINING = 3;
  int UPLOAD_FAILURE_DROP = 4;
  int UPLOAD_FAILURE_REQUEST_FAILED_WITH_EXCEPTION = 5;

  public static interface BackgroundUploadListener {
    public void onUploadSuccessful(File file);

    public void onUploadFailed(File file, int reason);
  }

  /**
   * Schedules a file batch for upload
   *
   * @files - are for normal files which are subject for quota checks, etc.
   * @priorityFiles - are scheduled first and all checks are skipped
   */
  public void uploadInBackground(
      List<File> files, List<File> priorityFiles, BackgroundUploadListener listener);

  public boolean canUpload();
}
