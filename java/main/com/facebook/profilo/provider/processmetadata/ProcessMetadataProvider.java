// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.provider.processmetadata;

import android.app.ActivityManager;
import android.content.Context;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import java.io.File;
import java.util.List;

public class ProcessMetadataProvider implements TraceOrchestrator.TraceProvider {

  private Context mContext;

  public ProcessMetadataProvider(Context context) {
    mContext = context;
  }

  @Override
  public void onEnable(TraceContext context, File extraDataFolder) {
    logProcessList();
  }

  @Override
  public void onDisable(TraceContext context, File extraDataFolder) {
    logProcessList();
  }

  private void logProcessList() {
    ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
    if (am == null) {
      return;
    }
    List<ActivityManager.RunningAppProcessInfo> infos = am.getRunningAppProcesses();
    String processes = null;
    if (infos != null) {
      StringBuilder sb = new StringBuilder();
      for (ActivityManager.RunningAppProcessInfo info : infos) {
        if (info.uid == android.os.Process.myUid()) {
          sb.append(info.processName + "(" + info.pid + "),");
        }
      }
      if (sb.length() != 0) {
        sb.deleteCharAt(sb.length() - 1); // delete ',' at end.
      }
      processes = sb.toString();
    }
    Logger.writeEntryWithStringWithNoMatch(
        ProfiloConstants.PROVIDER_PROFILO_SYSTEM,
        EntryType.PROCESS_LIST,
        0,
        0,
        "processes",
        processes);
  }
}
