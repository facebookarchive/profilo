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

public final class TraceContext implements Parcelable {

  public long traceId;
  public String encodedTraceId;
  public int controller;
  public Object controllerObject;
  public Object context;
  public long longContext;
  public int enabledProviders;
  public int cpuSamplingRateMs;
  public int flags;
  public int abortReason;

  public static final Parcelable.Creator<TraceContext> CREATOR = new
      Parcelable.Creator<TraceContext>() {
        public TraceContext createFromParcel(Parcel in) {
          return new TraceContext(in);
        }

        public TraceContext[] newArray(int size) {
          return new TraceContext[size];
        }
  };

  public TraceContext() {
  }

  public TraceContext(
      long traceId,
      String encodedTraceId,
      int controller,
      Object controllerObject,
      Object context,
      long longContext,
      int enabledProviders,
      int cpuSamplingRateMs,
      int flags,
      int abortReason) {
    this.traceId = traceId;
    this.encodedTraceId = encodedTraceId;
    this.controller = controller;
    this.controllerObject = controllerObject;
    this.context = context;
    this.longContext = longContext;
    this.enabledProviders = enabledProviders;
    this.cpuSamplingRateMs = cpuSamplingRateMs;
    this.flags = flags;
    this.abortReason = abortReason;
  }

  public TraceContext(
      long traceId,
      String encodedTraceId,
      int controller,
      Object controllerObject,
      Object context,
      long longContext,
      int enabledProviders,
      int cpuSamplingRateMs,
      int flags) {
    this(
        traceId,
        encodedTraceId,
        controller,
        controllerObject,
        context,
        longContext,
        enabledProviders,
        cpuSamplingRateMs,
        flags,
        (short) 0);
  }

  public TraceContext(TraceContext traceContext) {
    this(
        traceContext.traceId,
        traceContext.encodedTraceId,
        traceContext.controller,
        traceContext.controllerObject,
        traceContext.context,
        traceContext.longContext,
        traceContext.enabledProviders,
        traceContext.cpuSamplingRateMs,
        traceContext.flags,
        traceContext.abortReason);
  }

  public TraceContext(TraceContext traceContext, int abortReason) {
    this(
        traceContext.traceId,
        traceContext.encodedTraceId,
        traceContext.controller,
        traceContext.controllerObject,
        traceContext.context,
        traceContext.longContext,
        traceContext.enabledProviders,
        traceContext.cpuSamplingRateMs,
        traceContext.flags,
        abortReason);
  }

  public TraceContext(TraceContext traceContext, int controller, Object controllerObject) {
    this(
        traceContext.traceId,
        traceContext.encodedTraceId,
        controller,
        controllerObject,
        traceContext.context,
        traceContext.longContext,
        traceContext.enabledProviders,
        traceContext.cpuSamplingRateMs,
        traceContext.flags,
        traceContext.abortReason);
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
    this.cpuSamplingRateMs = src.readInt();
    this.flags = src.readInt();
    this.abortReason = src.readInt();
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
    dest.writeInt(cpuSamplingRateMs);
    dest.writeInt(this.flags);
    dest.writeInt(abortReason);
  }
}
