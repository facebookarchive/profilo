// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.core;

import com.facebook.profilo.config.SystemControlConfig;
import java.io.File;
import java.util.List;

public interface BackgroundUploadService {
  public static interface BackgroundUploadListener {
    public void onUploadSuccessful(File file);
    public void onUploadFailed(File file);
  }

  public void uploadInBackground(List<File> files, BackgroundUploadListener listener);

  /**
   * This is a special upload call that ignores constraint checks.  This should only be used for
   * cases that really know what they're doing - such as manual uploads in developer builds.
   */
  public void uploadInBackgroundSkipChecks(List<File> files, BackgroundUploadListener listener);

  public boolean canUpload();

  public void updateConstraints(SystemControlConfig systemControl);
}
