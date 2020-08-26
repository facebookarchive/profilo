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
package com.facebook.profilo.ipc;

import android.os.Parcel;
import android.os.Parcelable;
import com.facebook.profilo.config.Config;
import javax.annotation.Nullable;

public final class TraceContext implements Parcelable {

  // Not synced to secondary processes
  @Nullable public Config config;
  // Every time context which is shared via a Parcelable changes it's required to update
  // Action's version in {@see ProfiloBroadcastReceiver}.
  public long traceId;
  public String encodedTraceId;
  public int controller;
  @Nullable public Object controllerObject;
  @Nullable public Object context;
  public long longContext;
  public int enabledProviders;
  public int flags;
  public int abortReason;
  public int traceConfigIdx;
  public TraceConfigExtras mTraceConfigExtras;

  public static final Parcelable.Creator<TraceContext> CREATOR =
      new Parcelable.Creator<TraceContext>() {
        public TraceContext createFromParcel(Parcel in) {
          return new TraceContext(in);
        }

        public TraceContext[] newArray(int size) {
          return new TraceContext[size];
        }
      };

  public TraceContext() {}

  public TraceContext(
      long traceId,
      String encodedTraceId,
      @Nullable Config config,
      int controller,
      @Nullable Object controllerObject,
      @Nullable Object context,
      long longContext,
      int enabledProviders,
      int flags,
      int abortReason,
      int traceConfigIdx,
      TraceConfigExtras traceConfigExtras) {
    this.traceId = traceId;
    this.encodedTraceId = encodedTraceId;
    this.config = config;
    this.controller = controller;
    this.controllerObject = controllerObject;
    this.context = context;
    this.longContext = longContext;
    this.enabledProviders = enabledProviders;
    this.flags = flags;
    this.abortReason = abortReason;
    this.traceConfigIdx = traceConfigIdx;
    this.mTraceConfigExtras = traceConfigExtras;
  }

  public TraceContext(
      long traceId,
      String encodedTraceId,
      Config config,
      int controller,
      Object controllerObject,
      Object context,
      long longContext,
      int enabledProviders,
      int flags,
      int traceConfigIdx,
      TraceConfigExtras traceConfigExtras) {
    this(
        traceId,
        encodedTraceId,
        config,
        controller,
        controllerObject,
        context,
        longContext,
        enabledProviders,
        flags,
        (short) 0,
        traceConfigIdx,
        traceConfigExtras);
  }

  public TraceContext(TraceContext traceContext) {
    this(
        traceContext.traceId,
        traceContext.encodedTraceId,
        traceContext.config,
        traceContext.controller,
        traceContext.controllerObject,
        traceContext.context,
        traceContext.longContext,
        traceContext.enabledProviders,
        traceContext.flags,
        traceContext.abortReason,
        traceContext.traceConfigIdx,
        traceContext.mTraceConfigExtras);
  }

  public TraceContext(TraceContext traceContext, int abortReason) {
    this(
        traceContext.traceId,
        traceContext.encodedTraceId,
        traceContext.config,
        traceContext.controller,
        traceContext.controllerObject,
        traceContext.context,
        traceContext.longContext,
        traceContext.enabledProviders,
        traceContext.flags,
        abortReason,
        traceContext.traceConfigIdx,
        traceContext.mTraceConfigExtras);
  }

  public TraceContext(
      TraceContext traceContext, @Nullable Config config, int controller, Object controllerObject) {
    this(
        traceContext.traceId,
        traceContext.encodedTraceId,
        config,
        controller,
        controllerObject,
        traceContext.context,
        traceContext.longContext,
        traceContext.enabledProviders,
        traceContext.flags,
        traceContext.abortReason,
        traceContext.traceConfigIdx,
        traceContext.mTraceConfigExtras);
  }

  public TraceContext(Parcel src) {
    readFromParcel(src);
  }

  public void readFromParcel(Parcel src) {
    this.traceId = src.readLong();
    this.encodedTraceId = src.readString();
    this.controller = src.readInt();
    this.controllerObject = null;
    this.context = null;
    this.longContext = src.readLong();
    this.enabledProviders = src.readInt();
    this.flags = src.readInt();
    this.abortReason = src.readInt();
    this.traceConfigIdx = src.readInt();
    this.mTraceConfigExtras = TraceConfigExtras.CREATOR.createFromParcel(src);
  }

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(Parcel dest, int flags) {
    dest.writeLong(traceId);
    dest.writeString(encodedTraceId);
    dest.writeInt(controller);
    dest.writeLong(longContext);
    dest.writeInt(enabledProviders);
    dest.writeInt(this.flags);
    dest.writeInt(abortReason);
    dest.writeInt(traceConfigIdx);
    mTraceConfigExtras.writeToParcel(dest, flags);
  }
}
