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
package com.facebook.profilo.core;

import com.facebook.profilo.ipc.TraceContext;
import com.facebook.soloader.SoLoader;
import java.io.File;
import javax.annotation.Nullable;

/**
 * Provides a base class for TraceProviders aware of concurrent traces and changing enabled provider
 * bits.
 *
 * <p>The primary way to use this class is to only implement the {@link #enable()} and {@link
 * #disable()} calls. This is the preferred solution for "streaming" providers i.e. providers that
 * write to the trace as data becomes available, throughout the life cycle of the trace or traces.
 *
 * <p>{@link #enable()} will be called when the first trace that enabled providers supported by this
 * class is started. It will always have a matching {@link #disable()} call for that set of
 * providers.
 *
 * <p>{@link #disable()} will be called when the last trace for the current enabled providers set
 * ends.
 *
 * <p>If the set of enabled provider bits supported by this class changes due to a concurrent trace
 * starting or ending, the provider will have {@link #disable()} and {@link #enable()} called to
 * perform a full transition.
 *
 * <p>A secondary way to use this class is to implement empty {@link #enable()} and {@link
 * #disable()} methods and use {@link #onTraceStarted(TraceContext, File)} and {@link
 * #onTraceEnded(TraceContext, File)} to be notified of a trace starting or ending. These callbacks
 * do not check the supported provider bits and will always execute.
 *
 * <p>This is useful for providers that "dump" data at the beginning or end of a trace and want to
 * get triggered for every trace, only their enabling trace (by, for example, checking that the
 * ending trace context is the same as the one from {@link #getEnablingTraceContext()}), or any
 * other custom situation.
 */
public abstract class BaseTraceProvider extends TraceOrchestrator.TraceProvider {

  private int mSavedProviders;
  private TraceContext mEnablingContext;
  private File mExtraFolder;
  protected static final int EVERY_PROVIDER_CHANGE = 0xFFFFFFFF;

  @Nullable private String mSolib;
  private boolean mSolibInitialized;

  protected BaseTraceProvider() {
    this(null);
  }

  protected BaseTraceProvider(@Nullable String nativeLibrary) {
    mSolib = nativeLibrary;

    // if no native lib, treat it as initialized
    mSolibInitialized = nativeLibrary == null;
  }

  protected final void ensureSolibLoaded() {
    if (!mSolibInitialized) {
      synchronized (this) {
        if (!mSolibInitialized) {
          SoLoader.loadLibrary(mSolib);
          mSolibInitialized = true;
        }
      }
    }
  }

  @Override
  public final void onEnable(TraceContext context, File extraDataFolder) {
    ensureSolibLoaded();
    onTraceStarted(context, extraDataFolder);
    processStateChange(context, extraDataFolder);
  }

  @Override
  public final void onDisable(TraceContext context, File extraDataFolder) {
    ensureSolibLoaded();
    onTraceEnded(context, extraDataFolder);
    processStateChange(context, extraDataFolder);
  }

  private void processStateChange(TraceContext context, File extraDataFolder) {
    int providerMask = TraceEvents.enabledMask(getSupportedProviders());

    // Nothing changed - keep enabled
    if (mSavedProviders != 0 && TraceEvents.isEnabled(mSavedProviders)) {
      return;
    }
    // If provider was tracing stop first to reset the state
    if (mSavedProviders != 0) {
      disable();
      mEnablingContext = null;
      mExtraFolder = null;
    }
    // Current provider is on => enable
    if (providerMask != 0) {
      mEnablingContext = context;
      mExtraFolder = extraDataFolder;
      enable();
    }
    mSavedProviders = providerMask;
  }

  /**
   * Called when this provider is first enabled with a given set of provider bits.
   *
   * <p>May be called for a concurrent trace if the trace requires a different subset of supported
   * providers enabled.
   */
  protected abstract void enable();

  /** Called when this provider is disabled for a given set of provider bits. */
  protected abstract void disable();

  /**
   * Called when any trace starts, regardless of whether any supported provider is enabled or not.
   */
  protected void onTraceStarted(TraceContext context, File extraDataFolder) {
    // noop
  }

  /** Called when any trace ends, regardless of whether any supported provider is enabled or not. */
  protected void onTraceEnded(TraceContext context, File extraDataFolder) {
    // noop
  }

  /** Returns the trace context that first enabled this provider, if any. */
  @Nullable
  protected TraceContext getEnablingTraceContext() {
    return mEnablingContext;
  }

  /** Returns the extra data folder for the trace that enabled this provider, if any. */
  @Nullable
  protected File getExtraDataFolder() {
    return mExtraFolder;
  }

  /** @return Supported provider mask */
  protected abstract int getSupportedProviders();
}
