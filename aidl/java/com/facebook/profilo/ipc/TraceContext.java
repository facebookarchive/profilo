// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.profilo.ipc;

import android.os.Parcel;
import android.os.Parcelable;

public final class TraceContext implements Parcelable {

  public long traceId;
  public String encodedTraceId;
  public int controller;
  public Object controllerObject;
  public Object context;
  public int intContext;
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
      int intContext,
      int enabledProviders,
      int cpuSamplingRateMs,
      int flags,
      int abortReason) {
    this.traceId = traceId;
    this.encodedTraceId = encodedTraceId;
    this.controller = controller;
    this.controllerObject = controllerObject;
    this.context = context;
    this.intContext = intContext;
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
      int intContext,
      int enabledProviders,
      int cpuSamplingRateMs,
      int flags) {
    this(
      traceId,
      encodedTraceId,
      controller,
      controllerObject,
      context,
      intContext,
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
        traceContext.intContext,
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
      traceContext.intContext,
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
        traceContext.intContext,
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
    this.intContext= src.readInt();
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
    dest.writeInt(intContext);
    dest.writeInt(enabledProviders);
    dest.writeInt(cpuSamplingRateMs);
    dest.writeInt(this.flags);
    dest.writeInt(abortReason);
  }
}
