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

import static com.facebook.profilo.core.ProfiloConstants.TRACE_CONFIG_ID;
import static com.facebook.profilo.core.ProfiloConstants.TRACE_CONFIG_TRACE_CONFIG_ID_SWITCH;

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
import com.facebook.profilo.logger.BufferLogger;
import com.facebook.profilo.logger.FileManager;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.profilo.mmapbuf.core.MmapBufferManager;
import java.io.File;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;

@SuppressLint({"BadMethodUse-java.lang.Thread.start"})
public final class TraceOrchestrator
    implements TraceWriterListener,
        ConfigProvider.ConfigUpdateListener,
        TraceControl.TraceControlListener,
        BackgroundUploadService.BackgroundUploadListener,
        BaseTraceProvider.ExtraDataFileProvider {

  public static final String EXTRA_DATA_FOLDER_NAME = "extra";

  public interface ProfiloBridgeFactory {
    @Nullable
    BackgroundUploadService getUploadService();

    @Nullable
    TraceOrchestratorListener[] getListeners();
  }

  public static final String MAIN_PROCESS_NAME = "main";

  private static final String TAG = "Profilo/TraceOrchestrator";

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
        new TraceOrchestrator(
            context, configProvider, providers, processName, isMainProcess, traceFolder);

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
  @GuardedBy("this")
  private ConfigProvider mConfigProvider;

  @GuardedBy("this")
  @Nullable
  private volatile Config mConfig;

  @GuardedBy("this")
  private FileManager mFileManager;

  @GuardedBy("this")
  @Nullable
  private BackgroundUploadService mBackgroundUploadService;

  @GuardedBy("this")
  @Nullable
  private ProfiloBridgeFactory mProfiloBridgeFactory;

  @GuardedBy("this")
  private BaseTraceProvider[] mNormalTraceProviders;

  @GuardedBy("this")
  private BaseTraceProvider[] mSyncTraceProviders;

  private @Nullable MmapBufferManager mMmapBufferManager;

  private final Object mSyncProvidersLock = new Object();

  private final TraceListenerManager mListenerManager;
  private final boolean mIsMainProcess;
  private final String mProcessName;

  private final Random mRandom;

  // VisibleForTesting
  /*package*/ TraceOrchestrator(
      Context context,
      ConfigProvider configProvider,
      BaseTraceProvider[] traceProviders,
      String processName,
      boolean isMainProcess,
      @Nullable File traceFolder) {
    mConfigProvider = configProvider;
    mConfig = null;
    mFileManager = new FileManager(context, traceFolder);
    mBackgroundUploadService = null;
    mRandom = new Random();
    mListenerManager = new TraceListenerManager();
    mProcessName = processName;
    mIsMainProcess = isMainProcess;

    ArrayList<BaseTraceProvider> syncProviderList = new ArrayList<>();
    ArrayList<BaseTraceProvider> normalProviderList = new ArrayList<>();
    for (BaseTraceProvider provider : traceProviders) {
      if (provider.requiresSynchronousCallbacks()) {
        syncProviderList.add(provider);
        continue;
      }
      normalProviderList.add(provider);
    }
    mNormalTraceProviders =
        normalProviderList.toArray(new BaseTraceProvider[normalProviderList.size()]);
    mSyncTraceProviders = syncProviderList.toArray(new BaseTraceProvider[syncProviderList.size()]);
  }

  @SuppressLint({
    "BadMethodUse-android.os.HandlerThread._Constructor",
    "BadMethodUse-java.lang.Thread.start",
  })
  // VisibleForTesting
  /*package*/ void bind(Context context, SparseArray<TraceController> controllers) {
    Config initialConfig;
    synchronized (this) {
      initialConfig = mConfigProvider.getFullConfig();
      if (initialConfig == null) {
        throw new IllegalArgumentException("We only support v2 configs now!");
      }
    }

    File folder;
    synchronized (this) {
      folder = mFileManager.getFolder();
      mMmapBufferManager = new MmapBufferManager(mFileManager.getMmapBufferFolder());
      // process name is passed as the trace prefix
      TraceControl.initialize(
          controllers, this, initialConfig, mMmapBufferManager, folder, mProcessName, this);

      TraceEvents.initialize();

      // Complete a normal config update; this is somewhat wasteful but ensures consistency
      performConfigTransition(initialConfig);

      // Prepare the FileManager and start the worker thread
      // TODO: get these out of the config
      mFileManager.setMaxScheduledAge(TimeUnit.DAYS.toSeconds(1));
      mFileManager.setTrimThreshold(10);

      mListenerManager.addEventListener(new CoreTraceListener());
    }
  }

  @GuardedBy("this")
  public Config getConfig() {
    return mConfig;
  }

  private static BaseTraceProvider[] copyAndAppendProviderArray(
      BaseTraceProvider providerToAppend, BaseTraceProvider[] currentProviders) {
    BaseTraceProvider[] providers = Arrays.copyOf(currentProviders, currentProviders.length + 1);
    providers[providers.length - 1] = providerToAppend;
    return providers;
  }

  public synchronized void addTraceProvider(BaseTraceProvider provider) {
    if (provider.requiresSynchronousCallbacks()) {
      mSyncTraceProviders = copyAndAppendProviderArray(provider, mSyncTraceProviders);
      return;
    }
    mNormalTraceProviders = copyAndAppendProviderArray(provider, mNormalTraceProviders);
  }

  public void setConfigProvider(ConfigProvider newConfigProvider) {
    synchronized (this) {
      if (newConfigProvider.equals(mConfigProvider)) {
        return;
      }
    }

    mListenerManager.onNewConfigAvailable();
    performConfigProviderTransition(newConfigProvider);
  }

  public synchronized void setBackgroundUploadService(
      @Nullable BackgroundUploadService uploadService) {
    setBackgroundUploadService(uploadService, true /* triggerUpload */);
  }

  public synchronized void setBackgroundUploadService(
      @Nullable BackgroundUploadService uploadService, boolean triggerUpload) {
    if (mBackgroundUploadService == uploadService) {
      return;
    }
    mBackgroundUploadService = uploadService;
    if (triggerUpload) {
      triggerUpload();
    }
  }

  public synchronized void setProfiloBridgeFactory(ProfiloBridgeFactory profiloFactory) {
    mProfiloBridgeFactory = profiloFactory;
    TraceOrchestratorListener[] listeners = profiloFactory.getListeners();
    if (listeners != null) {
      for (TraceOrchestratorListener listener : listeners) {
        mListenerManager.addEventListener(listener);
      }
    }
  }

  public void addListener(TraceOrchestratorListener listener) {
    mListenerManager.addEventListener(listener);
  }

  public synchronized void removeListener(TraceOrchestratorListener listener) {
    mListenerManager.removeEventListener(listener);
  }

  @Override
  public void onConfigUpdated(Config config) {
    mListenerManager.onNewConfigAvailable();
    synchronized (this) {
      performConfigTransition(config);
    }
    mListenerManager.onConfigUpdated();
  }

  private void performConfigProviderTransition(ConfigProvider newConfigProvider) {
    synchronized (this) {
      mConfigProvider = newConfigProvider;
      performConfigTransition(newConfigProvider.getFullConfig());
    }
    mListenerManager.onConfigUpdated();
  }

  @GuardedBy("this")
  private void performConfigTransition(Config newConfig) {
    if (newConfig.equals(mConfig)) {
      return;
    }

    mConfig = newConfig;

    TraceControl traceControl = TraceControl.get();
    if (traceControl == null) {
      throw new IllegalStateException(
          "Performing config change before TraceControl has been initialized");
    }
    traceControl.setConfig(newConfig);
  }

  /**
   * Synchronous trace start with {@link TraceControl#startTrace(int, int, Object, int)}
   *
   * @param context
   */
  @Override
  public void onTraceStartSync(TraceContext context) {
    TraceEvents.refreshProviderNames();
    // Increment the providers
    TraceEvents.enableProviders(context.enabledProviders);

    // We want a subset of critical providers to run synchronously with trace start.
    // The rest of providers will run asynchronously.
    BaseTraceProvider[] syncProviders;
    synchronized (this) {
      syncProviders = mSyncTraceProviders;
    }
    synchronized (mSyncProvidersLock) {
      for (BaseTraceProvider provider : syncProviders) {
        provider.onEnable(context, this);
      }
    }
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
    String sanitizedTraceID = TraceIdSanitizer.sanitizeTraceId(context.encodedTraceId);
    File extraFolder = new File(new File(mainFolder, sanitizedTraceID), EXTRA_DATA_FOLDER_NAME);
    if (!extraFolder.isDirectory() && !extraFolder.mkdirs()) {
      // If the main & other process try to call mkdirs() simultaneously, one of the mkdirs will
      // fail
      // because mkdirs() checks for exists(). So, check if the directory was created.
      Log.w(
          TAG,
          "Failed to create extra data file! This could be because another process created it");
      if (!extraFolder.exists() || !extraFolder.isDirectory()) {
        return null;
      }
    }

    return new File(
        extraFolder, mProcessName + "-" + Process.myPid() + "-" + providerNames.iterator().next());
  }

  /** Asynchronous portion of trace start. Should include non-critical code for Trace startup. */
  @Override
  public void onTraceStartAsync(TraceContext context) {
    BaseTraceProvider[] providers;
    synchronized (this) {
      providers = mNormalTraceProviders;
    }

    for (BaseTraceProvider provider : providers) {
      provider.onEnable(context, this);
    }
    mListenerManager.onProvidersInitialized(context);

    mListenerManager.onTraceStart(context);
  }

  @Override
  public void onTraceStop(TraceContext context) {
    BaseTraceProvider[] normalProviders;
    BaseTraceProvider[] syncProviders;
    synchronized (this) {
      normalProviders = mNormalTraceProviders;
      syncProviders = mSyncTraceProviders;
    }
    if (mIsMainProcess) {
      boolean useTraceConfigId =
          context.mTraceConfigExtras.getBoolParam(TRACE_CONFIG_TRACE_CONFIG_ID_SWITCH, false);
      long configId = 0;
      if (useTraceConfigId) {
        String configIdStr = context.mTraceConfigExtras.getStringParam(TRACE_CONFIG_ID, null);
        if (configIdStr != null) {
          configId = Long.parseLong(configIdStr);
        }
      }
      if (configId == 0 && context.config != null) {
        configId = context.config.getID();
      }
      BufferLogger.writeStandardEntry(
          context.mainBuffer,
          Logger.FILL_TIMESTAMP | Logger.FILL_TID,
          EntryType.TRACE_ANNOTATION,
          ProfiloConstants.NONE,
          ProfiloConstants.NONE,
          Identifiers.CONFIG_ID,
          ProfiloConstants.NONE,
          configId);
    }

    int tracingProviders = 0;
    for (BaseTraceProvider provider : normalProviders) {
      tracingProviders |= provider.getActiveProviders();
    }
    for (BaseTraceProvider provider : syncProviders) {
      tracingProviders |= provider.getActiveProviders();
    }

    // Decrement providers for current trace
    TraceEvents.disableProviders(context.enabledProviders);

    synchronized (mSyncProvidersLock) {
      for (BaseTraceProvider provider : syncProviders) {
        provider.onDisable(context, this);
      }
    }
    for (BaseTraceProvider provider : normalProviders) {
      provider.onDisable(context, this);
    }
    mListenerManager.onProvidersStop(context, tracingProviders);
    mListenerManager.onTraceStop(context);
  }

  @Override
  public void onTraceAbort(TraceContext context) {
    BaseTraceProvider[] normalProviders;
    BaseTraceProvider[] syncProviders;
    synchronized (this) {
      normalProviders = mNormalTraceProviders;
      syncProviders = mSyncTraceProviders;
    }
    mListenerManager.onTraceAbort(context);

    // Decrement providers for current trace
    TraceEvents.disableProviders(context.enabledProviders);

    synchronized (mSyncProvidersLock) {
      for (BaseTraceProvider provider : syncProviders) {
        provider.onDisable(context, this);
      }
    }
    for (BaseTraceProvider provider : normalProviders) {
      provider.onDisable(context, this);
    }
  }

  @Override
  public void onTraceWriteStart(TraceContext trace) {
    mListenerManager.onTraceWriteStart(trace);
  }

  @Override
  public void onTraceWriteEnd(TraceContext trace) {
    try {
      mListenerManager.onTraceWriteEnd(trace);
      File traceFolder = trace.folder;
      if (!traceFolder.exists()) {
        return;
      }

      if (!mIsMainProcess) {
        // any file clean up will happen on the main process
        return;
      }

      handleZipAndUpload(trace);
      deleteDirectory(trace.folder);
    } finally {
      handleTraceWriteCompleted(trace);
    }
  }

  private void handleZipAndUpload(TraceContext trace) {
    File uploadFile = null;
    if (ZipHelper.shouldZipDirectory(trace.folder)) {
      File traceFile =
          ZipHelper.getCompressedFile(trace.folder, ZipHelper.ZIP_SUFFIX + ZipHelper.TMP_SUFFIX);

      // Add a timestamp to the file so that the FileManager's trimming rules
      // work fine (see trimFolderByFileCount)
      DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd'T'HH-mm-ss", Locale.US);
      String timestamp = dateFormat.format(new Date());
      File fileWithTimestamp =
          new File(traceFile.getParentFile(), timestamp + "-" + traceFile.getName());
      if (traceFile.renameTo(fileWithTimestamp)) {
        uploadFile = fileWithTimestamp;
      }
    } else {
      File[] fileList = trace.folder.listFiles();
      if (fileList == null || fileList.length == 0) {
        return;
      }
      File traceFile = fileList[0];
      // Move the trace file up to the base folder
      File targetTraceFile = new File(trace.folder.getParentFile(), traceFile.getName());
      if (traceFile.renameTo(targetTraceFile)) {
        uploadFile = targetTraceFile;
      }
    }
    if (uploadFile == null) {
      // Unable to prepare trace file for processing
      return;
    }

    boolean canUpload = mListenerManager.canUploadFlushedTrace(trace, uploadFile);
    if (!canUpload) {
      Log.d(TAG, "Not allowed to upload.");
      return;
    }

    FileManager.FileManagerStatistics fStats;
    synchronized (this) {
      boolean priority = (trace.flags & (Trace.FLAG_MANUAL | Trace.FLAG_MEMORY_ONLY)) != 0;
      mFileManager.addFileToUploads(uploadFile, priority);
      triggerUpload();
      fStats = mFileManager.getAndResetStatistics();
    }

    mListenerManager.onTraceScheduledForUpload(trace);
    // This is as good a time to do some analytics as any.
    mListenerManager.onTraceFlushedDoFileAnalytics(
        fStats.getTotalErrors(),
        fStats.getTrimmedDueToCount(),
        fStats.getTrimmedDueToAge(),
        fStats.getAddedFilesToUpload());
  }

  private void deleteDirectory(File folder) {
    try {
      ZipHelper.deleteDirectory(folder);
    } catch (Exception e) {
      Log.e(TAG, "failed to delete directory", e);
    }
  }

  @Override
  public void onTraceWriteAbort(TraceContext trace, int abortReason) {
    try {
      mListenerManager.onTraceWriteAbort(trace, abortReason);

      Log.w(TAG, "Trace is aborted with code: " + ProfiloConstants.abortReasonName(abortReason));
      TraceControl traceControl = TraceControl.get();
      if (traceControl == null) {
        throw new IllegalStateException("No TraceControl when cleaning up aborted trace");
      }
      traceControl.cleanupTraceContextByID(trace.traceId, abortReason);

      if (trace == null) {
        // Primarily covers the case where a trace threw an exception before onTraceStartAsync even
        // ran. We have done all we can (cleaned up the trace contexts above), let's bail out.
        return;
      }

      if (!mIsMainProcess) {
        // any file clean up will happen on the main process
        return;
      }

      File traceFolder = trace.folder;
      if (!traceFolder.exists()) {
        return;
      }

      boolean uploadTrace = false;

      Config config;
      synchronized (this) {
        config = mConfig;
      }

      if (config != null && abortReason == ProfiloConstants.ABORT_REASON_TIMEOUT) {
        int sampleRate =
            config.optSystemConfigParamInt(
                ProfiloConstants.SYSTEM_CONFIG_TIMED_OUT_UPLOAD_SAMPLE_RATE, 0);
        uploadTrace = sampleRate != 0 && mRandom.nextInt(sampleRate) == 0;
      }

      if (uploadTrace) {
        handleZipAndUpload(trace);
      }
      deleteDirectory(traceFolder);
    } finally {
      handleTraceWriteCompleted(trace);
    }
  }

  /** Common logic for aborts and normal ends coming from the trace writer thread. */
  private void handleTraceWriteCompleted(TraceContext trace) {
    for (Buffer buffer : trace.buffers) {
      if (!mMmapBufferManager.deallocateBuffer(buffer)) {
        Log.e(TAG, "Could not release memory for buffer for trace: " + trace.encodedTraceId);
      }
    }
  }

  @Override
  public void onTraceWriteException(TraceContext trace, Throwable t) {
    Log.e(TAG, "Write exception", t);
    mListenerManager.onTraceWriteException(trace, t);
    onTraceWriteAbort(trace, ProfiloConstants.ABORT_REASON_WRITER_EXCEPTION);
  }

  @Override
  public void onUploadSuccessful(File file) {
    synchronized (this) {
      mFileManager.handleSuccessfulUpload(file);
    }
    mListenerManager.onUploadSuccessful(file);
  }

  @Override
  public void onUploadFailed(File file, int reason) {
    mListenerManager.onUploadFailed(file, reason);
  }

  @GuardedBy("this")
  public void triggerUpload() {
    BackgroundUploadService backgroundUploadService = getUploadService(false /* triggerUpload */);
    if (backgroundUploadService == null) {
      return;
    }

    backgroundUploadService.uploadInBackground(
        mFileManager.getDefaultFilesToUpload(), mFileManager.getPriorityFilesToUpload(), this);
  }

  /**
   * Sets a new config provider and deletes all existing trace files.
   *
   * @return true if the cleanup succeeded, false otherwise
   */
  public synchronized boolean clearConfigurationAndTraces() {
    setConfigProvider(new DefaultConfigProvider());
    TraceControl tc = TraceControl.get();
    if (tc != null) {
      // Abort all the currently running traces
      for (TraceContext ctx : tc.getCurrentTraces()) {
        tc.abortTraceWithReason(
            ctx.controller, ctx.context, ctx.longContext, ProfiloConstants.ABORT_REASON_LOGOUT);
      }
    }
    return mFileManager.deleteAllFiles();
  }

  /** Returns all known trace files. Only use for crash reporting. */
  public synchronized Iterable<File> getAllTraceFilesForCrashReport() {
    return mFileManager.getAllFiles();
  }

  /**
   * Every time the background upload service, profilo listener, or config provider are touched, we
   * need to make sure we've initialized them because they are lazy-instantiated during cold start.
   */
  private synchronized @Nullable BackgroundUploadService getUploadService(boolean triggerUpload) {
    if (mBackgroundUploadService == null && mProfiloBridgeFactory != null) {
      BackgroundUploadService backgroundUploadService = mProfiloBridgeFactory.getUploadService();
      if (backgroundUploadService != null) {
        setBackgroundUploadService(backgroundUploadService, triggerUpload);
      }
    }
    return mBackgroundUploadService;
  }

  public String getProcessName() {
    return mProcessName;
  }

  public @Nullable MmapBufferManager getMmapBufferManager() {
    return mMmapBufferManager;
  }
}
