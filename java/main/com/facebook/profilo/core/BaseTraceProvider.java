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
package com.facebook.profilo.core;

import android.annotation.SuppressLint;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.MultiBufferLogger;
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
public abstract class BaseTraceProvider {

  public interface ExtraDataFileProvider {
    File getExtraDataFile(TraceContext context, BaseTraceProvider provider);
  }

  private int mSavedProviders;
  private TraceContext mEnablingContext;
  protected static final int EVERY_PROVIDER_CHANGE = 0xFFFFFFFF;

  @Nullable private String mSolib;
  private boolean mSolibInitialized;

  private volatile boolean mLoggerInitialized;
  @Nullable private MultiBufferLogger mLogger;

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
          MultiBufferLogger logger = getLogger();
          try {
            int id =
                logger.writeStandardEntry(
                    Logger.FILL_TID | Logger.FILL_TIMESTAMP,
                    EntryType.MARK_PUSH,
                    ProfiloConstants.NONE,
                    ProfiloConstants.NONE,
                    ProfiloConstants.NONE,
                    ProfiloConstants.NONE,
                    ProfiloConstants.NONE);
            logger.writeBytesEntry(
                ProfiloConstants.NONE, EntryType.STRING_NAME, id, "ensureSoLibLoaded: " + mSolib);

            SoLoader.loadLibrary(mSolib);
            mSolibInitialized = true;
          } finally {
            logger.writeStandardEntry(
                Logger.FILL_TID | Logger.FILL_TIMESTAMP,
                EntryType.MARK_POP,
                ProfiloConstants.NONE,
                ProfiloConstants.NONE,
                ProfiloConstants.NONE,
                ProfiloConstants.NONE,
                ProfiloConstants.NONE);
          }
        }
      }
    }
  }

  @SuppressLint("Return Not Nullable")
  protected final MultiBufferLogger getLogger() {
    if (mLoggerInitialized) {
      return mLogger;
    }
    synchronized (this) {
      if (!mLoggerInitialized) {
        mLogger = new MultiBufferLogger();
        mLoggerInitialized = true;
      }
    }
    return mLogger;
  }

  protected boolean isLoggerInitialized() {
    return mLoggerInitialized;
  }

  public final void onEnable(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    // Early exit if this trace does not affect this provider
    if ((context.enabledProviders & getSupportedProviders()) == 0) {
      return;
    }
    getLogger().addBuffer(context.mainBuffer);
    ensureSolibLoaded();
    processStateChange(context);
    onTraceStarted(context, dataFileProvider);
  }

  public final void onDisable(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    // Currently not tracing or trace does not affect provider
    if (mSavedProviders == 0 || (context.enabledProviders & getSupportedProviders()) == 0) {
      return;
    }
    ensureSolibLoaded();
    onTraceEnded(context, dataFileProvider);
    processStateChange(context);
    getLogger().removeBuffer(context.mainBuffer);
  }

  private void processStateChange(TraceContext context) {
    // Get the enabled providers across all traces. We need to know which ones of the
    // supported providers are enabled across all current traces.
    int providerMask = TraceEvents.enabledMask(getSupportedProviders());

    // Nothing changed - keep enabled
    if (mSavedProviders != 0 && TraceEvents.enabledMask(mSavedProviders) == mSavedProviders) {
      return;
    }
    // If provider was tracing stop first to reset the state
    if (mSavedProviders != 0) {
      disable();
      mEnablingContext = null;
    }
    // Current provider is on => enable
    if (providerMask != 0) {
      mEnablingContext = context;
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
  protected void onTraceStarted(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    // noop
  }

  /** Called when any trace ends, regardless of whether any supported provider is enabled or not. */
  protected void onTraceEnded(TraceContext context, ExtraDataFileProvider dataFileProvider) {
    // noop
  }

  /** Returns the trace context that first enabled this provider, if any. */
  @Nullable
  protected TraceContext getEnablingTraceContext() {
    return mEnablingContext;
  }

  /** @return Supported provider mask (internal API) */
  protected abstract int getSupportedProviders();

  /** @return Tracing provider mask (internal API) */
  protected abstract int getTracingProviders();

  /** @return Currently tracing providers bitmask. */
  public int getActiveProviders() {
    if (mSolib != null && !mSolibInitialized) {
      return 0; // Not initialized => not tracing
    }
    return getTracingProviders();
  }

  /**
   * Determines trace lifecycle callback semantics of an implementing provider.
   *
   * <p>Providers which return "true" are guaranteed that "onEnable" will be called synchronously
   * with trace start. Asynchronous execution means both "onEnable" and "onDisable" are processed on
   * a dedicated service thread. Providers which do not override this method are asynchronous by
   * default.
   *
   * @return true if synchronous and false otherwise
   */
  public boolean requiresSynchronousCallbacks() {
    return false;
  }
}
