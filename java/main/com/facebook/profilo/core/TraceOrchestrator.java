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

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.util.SparseArray;
import com.facebook.file.zip.ZipHelper;
import com.facebook.profilo.config.Config;
import com.facebook.profilo.config.ConfigProvider;
import com.facebook.profilo.config.DefaultConfigProvider;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.FileManager;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.LoggerCallbacks;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.writer.NativeTraceWriterCallbacks;
import java.io.File;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Random;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

@SuppressLint({"BadMethodUse-java.lang.Thread.start"})
public final class TraceOrchestrator
    implements NativeTraceWriterCallbacks,
        ConfigProvider.ConfigUpdateListener,
        TraceControl.TraceControlListener,
        BackgroundUploadService.BackgroundUploadListener,
        LoggerCallbacks {

  public static final String EXTRA_DATA_FOLDER_NAME = "extra";

  static final String CHECKSUM_DELIM = "-cs-";

  public interface TraceListener
      extends NativeTraceWriterCallbacks,
          BackgroundUploadService.BackgroundUploadListener,
          TraceControl.TraceControlListener,
          LoggerCallbacks {
    void onTraceFlushed(File trace);
    void onTraceFlushedDoFileAnalytics(
        int totalErrors,
        int trimmedFromCount,
        int trimmedFromAge,
        int filesAddedToUpload);
    /** Config update has been received but not yet applied. */
    void onBeforeConfigUpdate();
    /** New updated config has been applied. */
    void onAfterConfigUpdate();

    void onProvidersInitialized(TraceContext ctx);
  }

  public interface TraceProvider {

    /**
     * @param context - the TraceContext describing the active trace
     * @param extraDataFolder - A folder in which this provider can save extra data. May not exist
     *     yet. TraceProviders are encouraged to save temporary files elsewhere and only
     *     move/hardlink into this folder in {@link #onDisable(TraceContext, File)}.
     */
    void onEnable(TraceContext context, File extraDataFolder);

    /**
     * @param context - the TraceContext describing the active trace
     * @param extraDataFolder - the folder in which this provider can save extra data. May not exist
     *     yet.
     * @see #onEnable(TraceContext, File)
     */
    void onDisable(TraceContext context, File extraDataFolder);
  }

  public interface ProfiloBridgeFactory {
    BackgroundUploadService getUploadService();
    ConfigProvider getProvider();

    TraceListener[] getListeners();

    boolean isMultiProcessTracingEnabled();
  }

  class TraceEventsHandler extends Handler {
    static final int MSG_TRACE_START = 0;

    TraceEventsHandler(Looper looper) {
      super(looper);
    }

    @Override
    public void handleMessage(Message msg) {
      switch (msg.what) {
        case MSG_TRACE_START:
          traceStart((TraceContext) msg.obj);
          break;
        default:
          throw new IllegalArgumentException("Not supported message.");
      }
    }
  }

  public static final String MAIN_PROCESS_NAME = "main";

  private static final String TAG = "Profilo/TraceOrchestrator";
  private static final int RING_BUFFER_SIZE = 5000;
  private boolean mHasReadFromBridge = false;

  private final HashMap<Long, Trace> mTraces;

  // This field is expected to be infrequently accessed (just once for the Dummy -> DI dependency
  // shift, ideally), so using AtomicReference (and suffering the virtual get() call) is acceptable.
  private static final AtomicReference<TraceOrchestrator> sInstance = new AtomicReference<>(null);

  public static void initialize(
      Context context,
      @Nullable ConfigProvider configProvider,
      String processName,
      boolean isMainProcess,
      TraceProvider[] providers,
      SparseArray<TraceController> controllers) {
    if (configProvider == null) {
      configProvider = new DefaultConfigProvider();
    }

    TraceOrchestrator orchestrator = new TraceOrchestrator(
        context,
        configProvider,
        providers,
        isMainProcess);

    if (sInstance.compareAndSet(null, orchestrator)) {
      orchestrator.bind(context, controllers, processName);
    } else {
      throw new IllegalStateException("Orchestrator already initialized");
    }
  }

  public static boolean isInitialized() {
    return (sInstance.get() != null);
  }

  public static TraceOrchestrator get() {
    TraceOrchestrator orchestrator = sInstance.get();
    if (orchestrator == null) {
      throw new IllegalStateException("TraceOrchestrator has not been initialized");
    }
    return orchestrator;
  }

  // Current ConfigProvider and Config
  @GuardedBy("this") private ConfigProvider mConfigProvider;
  @GuardedBy("this") @Nullable private Config mConfig;
  // Future ConfigProvider : used if we updated Config/ConfigProvider during a trace.
  @GuardedBy("this") @Nullable ConfigProvider mNextConfigProvider;
  @GuardedBy("this") private FileManager mFileManager;
  @GuardedBy("this") @Nullable private BackgroundUploadService mBackgroundUploadService;

  @GuardedBy("this")
  @Nullable
  private ProfiloBridgeFactory mProfiloBridgeFactory;

  @GuardedBy("this") private TraceProvider[] mTraceProviders;

  private final TraceListenerManager mListenerManager;
  private final boolean mIsMainProcess;

  private final Random mRandom;

  private @Nullable TraceEventsHandler mHandler;

  //VisibleForTesting
  /*package*/ TraceOrchestrator(
      Context context,
      ConfigProvider configProvider,
      TraceProvider[] traceProviders,
      boolean isMainProcess) {
    mConfigProvider = configProvider;
    mTraceProviders = traceProviders;
    mConfig = null;
    mFileManager = new FileManager(context);
    mBackgroundUploadService = null;
    mRandom = new Random();
    mListenerManager = new TraceListenerManager();
    mIsMainProcess = isMainProcess;
    mTraces = new HashMap<>(2);
  }

  @SuppressLint({
    "BadMethodUse-android.os.HandlerThread._Constructor",
    "BadMethodUse-java.lang.Thread.start",
  })
  //VisibleForTesting
  /*package*/ void bind(
      Context context, SparseArray<TraceController> controllers, String processName) {
    Config initialConfig;
    synchronized (this) {
      mConfigProvider.setConfigUpdateListener(this);
      initialConfig = mConfigProvider.getFullConfig();
    }

    // Install the available TraceControllers
    TraceControl.initialize(controllers, this, initialConfig.getControllersConfig());

    File folder;
    synchronized (this) {
      folder = mFileManager.getFolder();

      // using process name as a unique prefix for each process
      Logger.initialize(RING_BUFFER_SIZE, folder, processName, this, this);

      // Complete a normal config update; this is somewhat wasteful but ensures consistency
      performConfigTransition(initialConfig);

      // Prepare the FileManager and start the worker thread
      // TODO: get these out of the config
      mFileManager.setMaxScheduledAge(TimeUnit.DAYS.toSeconds(1));
      mFileManager.setTrimThreshold(10);
    }
  }

  public void checkConfigProvider() {
    getConfigProvider();
  }

  @GuardedBy("this")
  public Config getConfig() {
    return mConfig;
  }

  synchronized public void addTraceProvider(TraceProvider provider) {
    TraceProvider[] providers = Arrays.copyOf(mTraceProviders, mTraceProviders.length + 1);
    providers[providers.length - 1] = provider;
    mTraceProviders = providers;
  }

  public void setConfigProvider(ConfigProvider newConfigProvider) {
    synchronized (this) {
      if (newConfigProvider.equals(mConfigProvider)) {
        return;
      }
    }

    mListenerManager.onBeforeConfigUpdate();

    synchronized (this) {
      // Defer updating the config provider if we're inside a trace
      TraceControl traceControl = TraceControl.get();
      if (traceControl != null && traceControl.isInsideTrace()) {
        mNextConfigProvider = newConfigProvider;
        return;
      }

      performConfigProviderTransition(newConfigProvider);
    }
  }

  public synchronized void setBackgroundUploadService(
      @Nullable BackgroundUploadService uploadService) {
    if (mBackgroundUploadService == uploadService) {
      return;
    }
    mBackgroundUploadService = uploadService;
    if (mBackgroundUploadService != null && mConfig != null) {
      mBackgroundUploadService.updateConstraints(mConfig.getSystemControl());
    }
    triggerUpload();
  }

  public synchronized void setProfiloBridgeFactory(ProfiloBridgeFactory profiloFactory) {
    mProfiloBridgeFactory = profiloFactory;
    TraceListener[] listeners = profiloFactory.getListeners();
    if (listeners != null) {
      for (TraceListener listener : listeners) {
        mListenerManager.addEventListener(listener);
      }
    }
  }

  public void addListener(TraceListener listener) {
    mListenerManager.addEventListener(listener);
  }

  public void removeListener(TraceListener listener) {
    mListenerManager.removeEventListener(listener);
  }

  @Override
  public void onConfigUpdated(Config config) {
    mListenerManager.onBeforeConfigUpdate();
    synchronized (this) {
      // Defer updating the config if we're inside a trace
      TraceControl traceControl = TraceControl.get();
      if (traceControl != null && traceControl.isInsideTrace()) {
        // OnTraceStop/Abort this will cause config to be updated.
        mNextConfigProvider = mConfigProvider;
        return;
      }
      performConfigTransition(config);
    }
  }

  private void performConfigProviderTransition(ConfigProvider newConfigProvider) {
    synchronized (this) {
      mConfigProvider.setConfigUpdateListener(null);
      newConfigProvider.setConfigUpdateListener(this);
      mConfigProvider = newConfigProvider;
      performConfigTransition(newConfigProvider.getFullConfig());
    }
    mListenerManager.onAfterConfigUpdate();
  }

  @GuardedBy("this")
  private void performConfigTransition(Config newConfig) {
    if (newConfig.equals(mConfig)) {
      return;
    }

    TraceEvents.clearAllProviders(); // disable everything
    // potentially schedule the following in the future, after a cool down period

    mConfig = newConfig;
    TraceControl traceControl = TraceControl.get();
    if (traceControl == null) {
      throw new IllegalStateException(
          "Performing config change before TraceControl has been initialized");
    }
    traceControl.setConfig(newConfig.getControllersConfig());

    BackgroundUploadService backgroundUploadService = getUploadService();

    if (backgroundUploadService != null) {
      backgroundUploadService.updateConstraints(newConfig.getSystemControl());
    }
  }

  private void ensureHandlerInitialized() {
    if (mHandler == null) {
      mHandler = new TraceEventsHandler(TraceControlThreadHolder.getInstance().getLooper());
    }
  }

  private File getSanitizedTraceFolder(TraceContext context) {
    File folder;
    synchronized (this) {
      folder = mFileManager.getFolder();
    }
    String sanitizedTraceID = context.encodedTraceId.replaceAll("[^a-zA-Z0-9\\-_.]", "_");
    folder = new File(folder, sanitizedTraceID);
    return folder;
  }

  /**
   * Synchronous trace start with {@link TraceControl#startTrace(int, int, Object, int)}
   *
   * @param context
   */
  @Override
  public void onTraceStart(TraceContext context) {
    // Increment the providers
    TraceEvents.enableProviders(context.enabledProviders);

    mListenerManager.onTraceStart(context);

    ensureHandlerInitialized();
    mHandler.obtainMessage(TraceEventsHandler.MSG_TRACE_START, context).sendToTarget();
  }

  /** Asynchronous portion of trace start. Should include non-critical code for Trace startup. */
  void traceStart(TraceContext context) {
    TraceProvider[] providers;
    synchronized (this) {
      providers = mTraceProviders;
    }
    File folder = new File(getSanitizedTraceFolder(context), EXTRA_DATA_FOLDER_NAME);
    for (TraceProvider provider : providers) {
      provider.onEnable(context, folder);
    }
    mListenerManager.onProvidersInitialized(context);
  }

  @Override
  public void onTraceStop(TraceContext context) {
    TraceProvider[] providers;
    Config config;
    synchronized (this) {
      providers = mTraceProviders;
      config = mConfig;
    }

    if (config != null) {
      Logger.writeEntryWithoutMatch(
          ProfiloConstants.PROVIDER_PROFILO_SYSTEM,
          EntryType.TRACE_ANNOTATION,
          Identifiers.CONFIG_ID,
          config.getConfigID());
    }

    // Decrement providers for current trace
    TraceEvents.disableProviders(context.enabledProviders);

    File folder = new File(getSanitizedTraceFolder(context), EXTRA_DATA_FOLDER_NAME);
    for (TraceProvider provider : providers) {
      provider.onDisable(context, folder);
    }

    checkConfigTransition();

    mListenerManager.onTraceStop(context);
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    checkConfigTransition();

    TraceProvider[] providers;
    synchronized (this) {
      providers = mTraceProviders;
    }
    mListenerManager.onTraceAbort(context);

    // Decrement providers for current trace
    TraceEvents.disableProviders(context.enabledProviders);

    File folder = new File(getSanitizedTraceFolder(context), EXTRA_DATA_FOLDER_NAME);
    for (TraceProvider provider : providers) {
      provider.onDisable(context, folder);
    }
  }

  private void checkConfigTransition() {
    ConfigProvider nextProvider;
    synchronized (this) {
      if (mNextConfigProvider == null) {
        return;
      }
      nextProvider = mNextConfigProvider;
      mNextConfigProvider = null;
    }
    performConfigProviderTransition(nextProvider);
  }

  @Override
  public void onTraceWriteStart(long traceId, int flags, String file) {
    Trace trace = mTraces.get(traceId);
    if (trace != null) {
      throw new IllegalStateException("Trace already registered on start");
    }
    mListenerManager.onTraceWriteStart(traceId, flags, file);
    mTraces.put(traceId, new Trace(traceId, flags, new File(file)));
  }

  @Override
  public void onTraceWriteEnd(long traceId, int crc) {
    Trace trace = mTraces.get(traceId);
    if (trace == null) {
      throw new IllegalStateException("onTraceWriteEnd can't be called without onTraceWriteStart");
    }
    mTraces.remove(traceId);
    mListenerManager.onTraceWriteEnd(traceId, crc);

    File logFile = trace.getLogFile();
    if (!logFile.exists()) {
      return;
    }

    String logFileName = logFile.getName();
    int extIndex = logFileName.lastIndexOf('.');
    String checksumSuffix = CHECKSUM_DELIM + Integer.toHexString(crc);
    File logFileWithCrc =
        new File(
            logFile.getParent(),
            (extIndex > 0 ? logFileName.substring(0, extIndex) : logFileName)
                + checksumSuffix
                + (extIndex > 0 ? logFileName.substring(extIndex) : ""));
    if (logFile.renameTo(logFileWithCrc)) {
      logFile = logFileWithCrc;
    }

    if (!mIsMainProcess) {
      // any file clean up will happen on the main process
      return;
    }

    File parent = logFile.getParentFile();
    File uploadFile;
    if (ZipHelper.shouldZipDirectory(parent)) {
      uploadFile = ZipHelper.getCompressedFile(parent, ZipHelper.ZIP_SUFFIX + ZipHelper.TMP_SUFFIX);
      ZipHelper.deleteDirectory(parent);
    } else {
      uploadFile = logFile;
    }
    if (uploadFile == null) {
      return;
    }

    uploadTrace(logFile, uploadFile, parent, trace.getFlags());
  }

  @Override
  public void onTraceWriteAbort(long traceId, int abortReason) {
    Trace trace = mTraces.get(traceId);
    if (trace == null) {
      throw new IllegalStateException(
          "onTraceWriteAbort can't be called without onTraceWriteStart");
    }
    mTraces.remove(traceId);
    mListenerManager.onTraceWriteAbort(traceId, abortReason);

    Log.w(TAG, "Trace is aborted with code: " + String.valueOf(abortReason));
    TraceControl traceControl = TraceControl.get();
    if (traceControl == null) {
      throw new IllegalStateException("No TraceControl when cleaning up aborted trace");
    }
    traceControl.cleanupTraceContextByID(traceId, abortReason);

    if (!mIsMainProcess) {
      // any file clean up will happen on the main process
      return;
    }

    File logFile = trace.getLogFile();
    if (!logFile.exists()) {
      return;
    }

    File parent = logFile.getParentFile();

    boolean uploadTrace = false;

    Config config;
    synchronized (this) {
      config = mConfig;
    }

    if (config != null && abortReason == ProfiloConstants.ABORT_REASON_TIMEOUT) {
      int sampleRate = config.getControllersConfig().getTimedOutUploadSampleRate();
      uploadTrace = sampleRate != 0 && mRandom.nextInt(sampleRate) == 0;
    }

    if (!uploadTrace && !logFile.delete()) {
      Log.e(TAG, "Could not delete aborted trace");
    }

    if (!uploadTrace) {
      ZipHelper.deleteDirectory(parent);
      return;
    }
    uploadTrace(logFile, logFile, parent, trace.getFlags());
  }

  private void uploadTrace(File logFile, File uploadFile, File parent, int traceFlags) {
    FileManager.FileManagerStatistics fStats;
    synchronized (this) {
      // Manual traces are untrimmable, everything else can be cleaned up
      boolean trimmable = (traceFlags & Trace.FLAG_MANUAL) == 0;
      mFileManager.addFileToUploads(uploadFile, trimmable);
      triggerUpload();
      fStats = mFileManager.getAndResetStatistics();
    }
    ZipHelper.deleteDirectory(parent);

    mListenerManager.onTraceFlushed(logFile);
    // This is as good a time to do some analytics as any.
    mListenerManager.onTraceFlushedDoFileAnalytics(
        fStats.getTotalErrors(),
        fStats.getTrimmedDueToCount(),
        fStats.getTrimmedDueToAge(),
        fStats.getAddedFilesToUpload());
  }

  @Override
  public void onUploadSuccessful(File file) {
    synchronized (this) {
      mFileManager.handleSuccessfulUpload(file);
    }
    mListenerManager.onUploadSuccessful(file);
  }

  @Override
  public void onLoggerException(Throwable t) {
    mListenerManager.onLoggerException(t);
  }

  @Override
  public void onUploadFailed(File file) {
    mListenerManager.onUploadFailed(file);
  }

  @GuardedBy("this")
  private void triggerUpload() {
    BackgroundUploadService backgroundUploadService = getUploadService();
    if (backgroundUploadService == null) {
        return;
    }

    backgroundUploadService.uploadInBackground(mFileManager.getTrimmableFilesToUpload(), this);
    backgroundUploadService.uploadInBackgroundSkipChecks(
        mFileManager.getUntrimmableFilesToUpload(),
        this);
  }

  /**
   * Sets a new config provider and deletes all existing trace files.
   *
   * @return true if the cleanup succeeded, false otherwise
   */
  synchronized public boolean clearConfigurationAndTraces(Context context) {
    setConfigProvider(new DefaultConfigProvider());
    return mFileManager.deleteAllFiles();
  }

  /**
   * Returns all known trace files. Only use for crash reporting.
   */
  synchronized public Iterable<File> getAllTraceFilesForCrashReport() {
    return mFileManager.getAllFiles();
  }

  /**
   * Every time the background upload service, profilo listener, or config provider are touched, we
   * need to make sure we've initialized them because they are lazy-instantiated during cold start.
   */
  private synchronized BackgroundUploadService getUploadService() {
    if (mBackgroundUploadService == null && mProfiloBridgeFactory != null) {
      BackgroundUploadService backgroundUploadService = mProfiloBridgeFactory.getUploadService();
      if (backgroundUploadService != null) {
        setBackgroundUploadService(backgroundUploadService);
      }
    }
    return mBackgroundUploadService;
  }

  synchronized private ConfigProvider getConfigProvider() {
    if (mHasReadFromBridge == false && mProfiloBridgeFactory != null) {
      // The config provider never starts as null, and overriding the config with the same config
      // has repercussions, so we need to make sure the config provider is only fetched from the
      // factory once.
      ConfigProvider configProvider = mProfiloBridgeFactory.getProvider();
      mHasReadFromBridge = true;

      if (configProvider != null) {
        setConfigProvider(configProvider);
      }
    }
    return mConfigProvider;
  }
}
