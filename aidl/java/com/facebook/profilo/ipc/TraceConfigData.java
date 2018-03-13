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

package com.facebook.profilo.ipc;

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
