// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.provider.packageinfo;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.Identifiers;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import javax.annotation.Nullable;

/** Logs versionName and versionCode from AndroidManifest.xml as trace annotations. */
public class PackageInfoProvider extends BaseTraceProvider {

  @Nullable private String mVersionName;
  private int mVersionCode;
  private final Context mContext;

  public PackageInfoProvider(Context context) {
    super();
    // Android can return null from Context::getApplicationContext.
    // Using the passed Context if it happens.
    Context appContext = context.getApplicationContext();
    mContext = appContext == null ? context : appContext;
  }

  private void resolvePackageInfo() {
    if (mVersionName != null) {
      return;
    }
    PackageManager pm = mContext.getPackageManager();
    if (pm == null) {
      return;
    }
    PackageInfo pi;
    try {
      pi = pm.getPackageInfo(mContext.getPackageName(), 0);
    } catch (PackageManager.NameNotFoundException e) {
      return;
    } catch (RuntimeException e) {
      return;
    }
    mVersionName = pi.versionName;
    mVersionCode = pi.versionCode;
  }

  @Override
  protected void enable() {}

  @Override
  protected void disable() {}

  @Override
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    resolvePackageInfo();

    if (mVersionName == null) {
      return;
    }

    int returnedMatchID =
        Logger.writeStandardEntry(
            ProfiloConstants.NONE,
            Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
            EntryType.TRACE_ANNOTATION,
            ProfiloConstants.NONE,
            ProfiloConstants.NONE,
            Identifiers.APP_VERSION_NAME,
            ProfiloConstants.NO_MATCH,
            (long) 0);
    if ("App version" != null) {
      returnedMatchID =
          Logger.writeBytesEntry(
              ProfiloConstants.NONE,
              Logger.SKIP_PROVIDER_CHECK,
              EntryType.STRING_KEY,
              returnedMatchID,
              "App version");
    }
    Logger.writeBytesEntry(
        ProfiloConstants.NONE,
        Logger.SKIP_PROVIDER_CHECK,
        EntryType.STRING_VALUE,
        returnedMatchID,
        mVersionName);

    Logger.writeStandardEntry(
        ProfiloConstants.NONE,
        Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
        EntryType.TRACE_ANNOTATION,
        ProfiloConstants.NONE,
        ProfiloConstants.NONE,
        Identifiers.APP_VERSION_CODE,
        ProfiloConstants.NONE,
        (long) mVersionCode);
  }

  @Override
  protected int getSupportedProviders() {
    return EVERY_PROVIDER_CHANGE;
  }

  @Override
  protected int getTracingProviders() {
    return 0;
  }
}
