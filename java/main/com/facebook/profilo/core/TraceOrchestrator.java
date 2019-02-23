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
import android.os.Process;
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
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.HashMap;
import java.util.Locale;
import java.util.Random;
import java.util.Set;
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
        LoggerCallbacks,
        BaseTraceProvider.ExtraDataFileProvider {

  public static final String EXTRA_DATA_FOLDER_NAME = "extra";

  static final String CHECKSUM_DELIM = "-cs-";

  public interface TraceListener
      extends NativeTraceWriterCallbacks,
          BackgroundUploadService.BackgroundUploadListener,
          LoggerCallbacks {

    void onTraceStart(TraceContext context);

    void onTraceStop(TraceContext context);

    void onTraceAbort(TraceContext context);

    void onTraceFlushed(File trace, long traceId);

    void onTraceFlushedDoFileAnalytics(
        int totalErrors,
        int trimmedFromCount,
        int trimmedFromAge,
        int filesAddedToUpload);
    /** Config update has been received but not yet applied. */
    void onBeforeConfigUpdate();
    /** New updated config has been applied. */
    void onAfterConfigUpdate();

    void onProvidersInitialized();

    void onProvidersStop(int activeProviders);
  }

  public interface ProfiloBridgeFactory {
    BackgroundUploadService getUploadService();
    ConfigProvider getProvider();

    TraceListener[] getListeners();
  }

  public static final String MAIN_PROCESS_NAME = "main";

  private static final String TAG = "Profilo/TraceOrchestrator";
  private static final int RING_BUFFER_SIZE_MAIN_PROCESS = 5000;
  private static final int RING_BUFFER_SIZE_SECONDARY_PROCESS = 1000;
  private boolean mHasReadFromBridge = false;

  private final HashMap<Long, Trace> mTraces;

  // This field is expected to be infrequently accessed (just once for the Dummy -> DI dependency
  // shift, ideally), so using AtomicReference (and suffering the virtual get() call) is acceptable.
  private static AtomicReference<TraceOrchestrator> sInstance = new AtomicReference<>(null);

  public static void initialize(
      Context context,
      @Nullable ConfigProvider configProvider,
      String processName,
      boolean isMainProcess,
      BaseTraceProvider[] providers,
      SparseArray<TraceController> controllers,
      @Nullable File traceFolder) {
    if (configProvider == null) {
      configProvider = new DefaultConfigProvider();
    }

    TraceOrchestrator orchestrator =
        new TraceOrchestrator(context, configProvider, providers, processName, isMainProcess, traceFolder);

    if (sInstance.compareAndSet(null, orchestrator)) {
      orchestrator.bind(context, controllers);
    } else {
      throw new IllegalStateException("Orchestrator already initialized");
    }
  }

  public FileManager getFileManager() {
    return mFileManager;
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

  @GuardedBy("this")
  private BaseTraceProvider[] mBaseTraceProviders;

  private final TraceListenerManager mListenerManager;
  private final boolean mIsMainProcess;
  private final String mProcessName;

  private final Random mRandom;

  // VisibleForTesting
  /*package*/ TraceOrchestrator(
      Context context,
      ConfigProvider configProvider,
      BaseTraceProvider[] BaseTraceProviders,
      String processName,
      boolean isMainProcess,
      @Nullable File traceFolder) {
    mConfigProvider = configProvider;
    mBaseTraceProviders = BaseTraceProviders;
    mConfig = null;
    mFileManager = new FileManager(context, traceFolder);
    mBackgroundUploadService = null;
    mRandom = new Random();
    mListenerManager = new TraceListenerManager();
    mProcessName = processName;
    mIsMainProcess = isMainProcess;
    mTraces = new HashMap<>(2);
  }

  @SuppressLint({
    "BadMethodUse-android.os.HandlerThread._Constructor",
    "BadMethodUse-java.lang.Thread.start",
  })
  //VisibleForTesting
  /*package*/ void bind(
      Context context, SparseArray<TraceController> controllers) {
    Config initialConfig;
    synchronized (this) {
      mConfigProvider.setConfigUpdateListener(this);
      initialConfig = mConfigProvider.getFullConfig();
    }

    // Install the available TraceControllers
    TraceControl.initialize(controllers, this, initialConfig);

    File folder;
    synchronized (this) {
      folder = mFileManager.getFolder();

      // using process name as a unique prefix for each process
      Logger.initialize(
          mIsMainProcess ? RING_BUFFER_SIZE_MAIN_PROCESS : RING_BUFFER_SIZE_SECONDARY_PROCESS,
          folder,
          mProcessName,
          this,
          this);

      // Complete a normal config update; this is somewhat wasteful but ensures consistency
      performConfigTransition(initialConfig);

      // Prepare the FileManager and start the worker thread
      // TODO: get these out of the config
      mFileManager.setMaxScheduledAge(TimeUnit.DAYS.toSeconds(1));
      mFileManager.setTrimThreshold(10);

      mListenerManager.addEventListener(new CoreTraceListener());
    }
  }

  public void checkConfigProvider() {
    getConfigProvider();
  }

  @GuardedBy("this")
  public Config getConfig() {
    return mConfig;
  }

  public synchronized void addTraceProvider(BaseTraceProvider provider) {
    BaseTraceProvider[] providers =
        Arrays.copyOf(mBaseTraceProviders, mBaseTraceProviders.length + 1);
    providers[providers.length - 1] = provider;
    mBaseTraceProviders = providers;
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
    traceControl.setConfig(newConfig);

    BackgroundUploadService backgroundUploadService = getUploadService();

    if (backgroundUploadService != null) {
      backgroundUploadService.updateConstraints(newConfig.getSystemControl());
    }
  }

  /**
   * Synchronous trace start with {@link TraceControl#startTrace(int, int, Object, int)}
   *
   * @param context
   */
  @Override
  public void onTraceStartSync(TraceContext context) {
    // Increment the providers
    TraceEvents.enableProviders(context.enabledProviders);

    // We want all the in-line providers to know that they're on.
    // However, don't do anything else blockingly.
  }

  @Nullable
  public File getExtraDataFile(TraceContext context, BaseTraceProvider provider) {
    if ((context.flags & Trace.FLAG_MEMORY_ONLY) != 0) {
      // Memory-only traces are not allowed to write to file system, hence skipping extra folder
      // creation.
      return null;
    }

    int providerId = provider.getSupportedProviders();
    Set<String> providerNames = ProvidersRegistry.getRegisteredProvidersByBitMask(providerId);
    if (providerNames.isEmpty()) {
      return null;
    }

    File mainFolder;
    synchronized (this) {
      mainFolder = mFileManager.getFolder();
    }
    String sanitizedTraceID = context.encodedTraceId.replaceAll("[^a-zA-Z0-9\\-_.]", "_");
    File extraFolder = new File(new File(mainFolder, sanitizedTraceID), EXTRA_DATA_FOLDER_NAME);
    if (!extraFolder.isDirectory() && !extraFolder.mkdirs()) {
      return null;
    }

    return new File(
        extraFolder, mProcessName + "-" + Process.myPid() + "-" + providerNames.iterator().next());
  }

  /** Asynchronous portion of trace start. Should include non-critical code for Trace startup. */
  @Override
  public void onTraceStartAsync(TraceContext context) {
    mListenerManager.onTraceStart(context);

    BaseTraceProvider[] providers;
    synchronized (this) {
      providers = mBaseTraceProviders;
    }
    for (BaseTraceProvider provider : providers) {
      provider.onEnable(context, this);
    }
    mListenerManager.onProvidersInitialized();
  }

  @Override
  public void onTraceStop(TraceContext context) {
    BaseTraceProvider[] providers;
    Config config;
    synchronized (this) {
      providers = mBaseTraceProviders;
      config = mConfig;
    }

    if (config != null) {
      Logger.writeStandardEntry(
          ProfiloConstants.NONE,
          Logger.SKIP_PROVIDER_CHECK | Logger.FILL_TIMESTAMP | Logger.FILL_TID,
          EntryType.TRACE_ANNOTATION,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          Identifiers.CONFIG_ID,
          ProfiloConstants.NONE,
          config.getConfigID());
    }

    // Decrement providers for current trace
    TraceEvents.disableProviders(context.enabledProviders);

    int tracingProviders = 0;
    for (BaseTraceProvider provider : providers) {
      tracingProviders |= provider.getActiveProviders();
      provider.onDisable(context, this);
    }
    mListenerManager.onProvidersStop(tracingProviders);

    checkConfigTransition();

    mListenerManager.onTraceStop(context);
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    checkConfigTransition();

    BaseTraceProvider[] providers;
    synchronized (this) {
      providers = mBaseTraceProviders;
    }
    mListenerManager.onTraceAbort(context);

    // Decrement providers for current trace
    TraceEvents.disableProviders(context.enabledProviders);

    for (BaseTraceProvider provider : providers) {
      provider.onDisable(context, this);
    }
  }

  private void checkConfigTransition() {
    ConfigProvider nextProvider;
    synchronized (this) {
      if (mNextConfigProvider == null) {
        return;
      }
      TraceControl traceControl = TraceControl.get();
      if (traceControl != null && traceControl.isInsideTrace()) {
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

      // Add a timestamp to the file so that the FileManager's trimming rules
      // work fine (see trimFolderByFileCount)
      DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH-mm-ss", Locale.US);
      String timestamp = dateFormat.format(new Date());
      File fileWithTimestamp =
          new File(uploadFile.getParentFile(), timestamp + "-" + uploadFile.getName());
      if (uploadFile.renameTo(fileWithTimestamp)) {
        uploadFile = fileWithTimestamp;
      }
      ZipHelper.deleteDirectory(parent);
    } else {
      uploadFile = logFile;
    }
    if (uploadFile == null) {
      return;
    }

    uploadTrace(logFile, uploadFile, parent, trace.getFlags(), traceId);
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

    Log.w(TAG, "Trace is aborted with code: " + ProfiloConstants.abortReasonName(abortReason));
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
    uploadTrace(logFile, logFile, parent, trace.getFlags(), traceId);
  }

  private void uploadTrace(
      File logFile, File uploadFile, File parent, int traceFlags, long traceId) {
    FileManager.FileManagerStatistics fStats;
    synchronized (this) {
      // Manual traces are untrimmable, everything else can be cleaned up
      boolean trimmable = (traceFlags & Trace.FLAG_MANUAL) == 0;
      mFileManager.addFileToUploads(uploadFile, trimmable);
      triggerUpload();
      fStats = mFileManager.getAndResetStatistics();
    }
    ZipHelper.deleteDirectory(parent);

    mListenerManager.onTraceFlushed(logFile, traceId);
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
