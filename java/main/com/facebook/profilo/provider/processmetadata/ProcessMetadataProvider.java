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

package com.facebook.profilo.provider.processmetadata;

import android.app.ActivityManager;
import android.content.Context;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import java.util.List;

public final class ProcessMetadataProvider extends BaseTraceProvider {

  private Context mContext;

  public ProcessMetadataProvider(Context context) {
    mContext = context;
  }

  @Override
  protected void onTraceStarted(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    logProcessList();
  }

  @Override
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    logProcessList();
  }

  @Override
  protected void enable() {}

  @Override
  protected void disable() {}

  @Override
  protected int getSupportedProviders() {
    return EVERY_PROVIDER_CHANGE;
  }

  @Override
  protected int getTracingProviders() {
    return 0;
  }

  private void logProcessList() {
    List<ActivityManager.RunningAppProcessInfo> infos;
    try {
      ActivityManager am = (ActivityManager) mContext.getSystemService(Context.ACTIVITY_SERVICE);
      if (am == null) {
        return;
      }
      infos = am.getRunningAppProcesses();
    } catch (Throwable ex) {
      // IPC calls can fail for seemingly random reasons. Various layers in the
      // framework end up wrapping the exceptions, so we can't look for the
      // specific type.
      // In any case, there's nothing we can do here.
      return;
    }

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
    if (processes == null || processes.isEmpty()) {
      processes = "PROCESS_METADATA_PROVIDER_FAILED_TO_GET_PROCESS_LIST";
    }
    int returnedMatchID =
        Logger.writeStandardEntry(
            ProfiloConstants.NONE,
            Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.PROCESS_LIST,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE);
    if ("processes" != null) {
      returnedMatchID =
          Logger.writeBytesEntry(
              ProfiloConstants.NONE,
              Logger.SKIP_PROVIDER_CHECK,
              EntryType.STRING_KEY,
              returnedMatchID,
              "processes");
    }
    Logger.writeBytesEntry(
        ProfiloConstants.NONE,
        Logger.SKIP_PROVIDER_CHECK,
        EntryType.STRING_VALUE,
        returnedMatchID,
        processes);
  }
}
