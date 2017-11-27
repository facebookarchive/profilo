// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.ipc;

import android.os.Parcel;
import android.os.Parcelable;

public final class TraceConfigData implements Parcelable {

  public long configID;
  // RootControllerConfig
  public int traceTimeoutMs;
  public int timedOutUploadSampleRate;

  public static final Parcelable.Creator<TraceConfigData> CREATOR = new
      Parcelable.Creator<TraceConfigData>() {
        public TraceConfigData createFromParcel(Parcel in) {
          return new TraceConfigData(in);
        }

        public TraceConfigData[] newArray(int size) {
          return new TraceConfigData[size];
        }
  };

  public TraceConfigData() {
  }

  public TraceConfigData(
      long configID,
      int traceTimeoutMs,
      int timedOutUploadSampleRate) {
    this.configID = configID;
    this.traceTimeoutMs = traceTimeoutMs;
    this.timedOutUploadSampleRate = timedOutUploadSampleRate;
  }

  public TraceConfigData(Parcel src) {
    readFromParcel(src);
  }

  public void readFromParcel(Parcel src) {
    this.configID = src.readLong();
    this.traceTimeoutMs = src.readInt();
    this.timedOutUploadSampleRate = src.readInt();
  }

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags) {
    dest.writeLong(configID);
    dest.writeInt(traceTimeoutMs);
    dest.writeInt(timedOutUploadSampleRate);
  }
}
