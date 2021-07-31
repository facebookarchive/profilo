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

import static org.assertj.core.api.Java6Assertions.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.util.SparseArray;
import com.facebook.profilo.config.Config;
import com.facebook.profilo.config.ConfigImpl;
import com.facebook.profilo.config.ConfigParams;
import com.facebook.profilo.ipc.TraceConfigExtras;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.FileManager;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.mmapbuf.Buffer;
import com.facebook.profilo.util.TestConfigProvider;
import com.facebook.profilo.util.TraceContextRule;
import com.facebook.testing.robolectric.v4.WithTestDefaultsRunner;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;
import org.fest.util.Files;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.powermock.reflect.Whitebox;

@RunWith(WithTestDefaultsRunner.class)
public class TraceOrchestratorTest {

  // These values are the inverse of each other, so we can do just one TraceEvents.isEnabled call
  private static final int DEFAULT_TRACING_PROVIDERS = 0xffffffff;
  private static final int SECOND_TRACE_TRACING_PROVIDERS = 0b11011;
  private static final int NORMAL_PROVIDER_TRACING_PROVIDERS = 0b001;
  private static final int NORMAL_PROVIDER_SUPPORTED_PROVIDERS = 0b101;
  private static final int SYNC_PROVIDER_SUPPORTED_PROVIDERS = 0b10;
  private static final int SYNC_PROVIDER_TRACING_PROVIDERS = 0b10;
  private static final String DEFAULT_TEMP_DIR = "temp_dir";

  private BackgroundUploadService mUploadService;
  private Context mContext;
  private FileManager mFileManager;
  private TestConfigProvider mConfigProvider;
  private TraceOrchestrator mOrchestrator;
  private TraceControl mTraceControl;
  private TraceContext mTraceContext;
  private TraceContext mSecondTraceContext;

  private BaseTraceProvider mTraceProvider;
  private BaseTraceProvider mBaseTraceProvider;
  private BaseTraceProvider mSyncBaseTraceProvider;

  private BaseTraceProvider[] mTraceProviders;

  @Rule public TraceContextRule mTraceContextRule = new TraceContextRule(0);

  static class TestBaseProvider extends BaseTraceProvider {

    final int supportedProviders;
    final int tracingProviders;
    final boolean sync;

    TestBaseProvider(int supportedProviders, int tracingProviders, boolean sync) {
      this.supportedProviders = supportedProviders;
      this.tracingProviders = tracingProviders;
      this.sync = sync;
    }

    @Override
    protected void enable() {}

    @Override
    protected void disable() {}

    @Override
    protected int getSupportedProviders() {
      return supportedProviders;
    }

    @Override
    protected int getTracingProviders() {
      return tracingProviders;
    }

    @Override
    public boolean requiresSynchronousCallbacks() {
      return sync;
    }
  }

  private Config buildConfigWithTimedOutSampleRate(int rate) {
    ConfigParams systemConfig = new ConfigParams();
    systemConfig.intParams = new TreeMap<>();
    systemConfig.intParams.put(ProfiloConstants.SYSTEM_CONFIG_TIMED_OUT_UPLOAD_SAMPLE_RATE, rate);
    return new ConfigImpl(0, systemConfig);
  }

  @Before
  public void setUp() throws Exception {
    mContext = mock(Context.class);
    mFileManager = mock(FileManager.class);
    mUploadService = mock(BackgroundUploadService.class);
    mConfigProvider = new TestConfigProvider();

    Buffer buffer = mTraceContextRule.newBuffer();
    mTraceContext =
        new TraceContext(
            0xFACEB00C, // traceId
            "FACEBOOOK", // encodedTraceId
            mConfigProvider.getFullConfig(), // config
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            DEFAULT_TRACING_PROVIDERS,
            1, // flags
            0,
            TraceConfigExtras.EMPTY,
            buffer,
            new Buffer[] {buffer},
            new File("."),
            "prefix-");

    buffer = mTraceContextRule.newBuffer();
    mSecondTraceContext =
        new TraceContext(
            0xFACEB000, // traceId
            "FACEBOOO0", // encodedTraceId
            mConfigProvider.getFullConfig(), // config
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            SECOND_TRACE_TRACING_PROVIDERS,
            1, // flags
            0,
            TraceConfigExtras.EMPTY,
            buffer,
            new Buffer[] {buffer},
            new File("."),
            "prefix-"); // mTraceConfigExtras

    mTraceProvider = mock(BaseTraceProvider.class);
    mBaseTraceProvider =
        spy(
            new TestBaseProvider(
                NORMAL_PROVIDER_SUPPORTED_PROVIDERS, NORMAL_PROVIDER_TRACING_PROVIDERS, false));
    mSyncBaseTraceProvider =
        spy(
            new TestBaseProvider(
                SYNC_PROVIDER_SUPPORTED_PROVIDERS, SYNC_PROVIDER_TRACING_PROVIDERS, true));

    mTraceProviders =
        new BaseTraceProvider[] {mTraceProvider, mBaseTraceProvider, mSyncBaseTraceProvider};
    when(mFileManager.getAndResetStatistics())
        .thenReturn(mock(FileManager.FileManagerStatistics.class));

    // This is gross but it beats using PowerMock to override the static .get() method
    Whitebox.setInternalState(TraceControl.class, "sInstance", (Object) null);
    mOrchestrator =
        new TraceOrchestrator(
            mContext,
            mConfigProvider,
            mTraceProviders,
            TraceOrchestrator.MAIN_PROCESS_NAME,
            true, // isMainProcess
            null); // Default trace location
    mOrchestrator.bind(mContext, new SparseArray<TraceController>(1));

    mTraceControl = mock(TraceControl.class);
    Whitebox.setInternalState(TraceControl.class, "sInstance", mTraceControl);

    // Also gross but meh
    Whitebox.setInternalState(mOrchestrator, "mFileManager", mFileManager);

    TraceEvents.clearAllProviders();
  }

  @Test
  public void testSetBackgroundUploadServiceTriggersImmediateUpload() {
    List<File> tracesList = new ArrayList<>();
    List<File> tracesListSkipChecks = new ArrayList<>();

    when(mFileManager.getTrimmableFilesToUpload()).thenReturn(tracesList);
    when(mFileManager.getUntrimmableFilesToUpload()).thenReturn(tracesListSkipChecks);

    mOrchestrator.setBackgroundUploadService(mUploadService);
    verify(mUploadService, times(1))
        .uploadInBackground(
            same(tracesList),
            same(tracesListSkipChecks),
            any(BackgroundUploadService.BackgroundUploadListener.class));
  }

  @Test
  public void testLazyUploadServiceInitTriggersImmediateUpload() throws Exception {
    List<File> tracesList = new ArrayList<>();
    List<File> tracesListSkipChecks = new ArrayList<>();

    when(mFileManager.getTrimmableFilesToUpload()).thenReturn(tracesList);
    when(mFileManager.getUntrimmableFilesToUpload()).thenReturn(tracesListSkipChecks);

    TraceOrchestrator.ProfiloBridgeFactory profiloBridgeFactory =
        mock(TraceOrchestrator.ProfiloBridgeFactory.class);
    when(profiloBridgeFactory.getUploadService()).thenReturn(mUploadService);

    mOrchestrator.setProfiloBridgeFactory(profiloBridgeFactory);
    Whitebox.invokeMethod(mOrchestrator, "triggerUpload");
    verify(mUploadService, times(1))
        .uploadInBackground(
            same(tracesList),
            same(tracesListSkipChecks),
            any(BackgroundUploadService.BackgroundUploadListener.class));
  }

  @Test
  public void testConfigChangeInstallsNewConfig() {
    reset(mTraceControl);
    mConfigProvider = new TestConfigProvider(new ConfigImpl(0, new ConfigParams()));
    mOrchestrator.onConfigUpdated(mConfigProvider.getFullConfig());

    assertThatAllProvidersDisabled();
    verify(mTraceControl).setConfig(any(Config.class));
  }

  @Test
  public void testListenersAreCalledOnConfigChange() {
    reset(mTraceControl);
    TraceListenerManager listenerManager = mock(TraceListenerManager.class);
    Whitebox.setInternalState(mOrchestrator, "mListenerManager", listenerManager);

    mOrchestrator.onConfigUpdated(mConfigProvider.getFullConfig());

    verify(listenerManager).onNewConfigAvailable();
    verify(listenerManager).onConfigUpdated();
  }

  @Test
  public void testTracingChangesProviders() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStartSync(mTraceContext);
    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isTrue();
  }

  @Test
  public void testMultiTracingChangesProviders() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStartSync(mTraceContext);
    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isTrue();
    mOrchestrator.onTraceStartSync(mSecondTraceContext);
    mOrchestrator.onTraceStop(mTraceContext);
    assertThat(TraceEvents.isEnabled(SECOND_TRACE_TRACING_PROVIDERS)).isTrue();
    mOrchestrator.onTraceStop(mSecondTraceContext);
    assertThatAllProvidersDisabled();
  }

  @Test
  public void testTraceStopChangesProviders() {
    mOrchestrator.onTraceStartSync(mTraceContext);
    mOrchestrator.onTraceStartAsync(mTraceContext);
    mOrchestrator.onTraceStop(mTraceContext);

    verifyProvidersDisabled(mTraceProviders);
    assertThatAllProvidersDisabled();
  }

  @Test
  public void testTraceAbortChangesProviders() {
    mOrchestrator.onTraceStartSync(mTraceContext);
    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isTrue();
    TraceContext traceContext =
        new TraceContext(mTraceContext, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);
    mOrchestrator.onTraceAbort(traceContext);
    assertThatAllProvidersDisabled();
    verifyProvidersDisabled(mTraceProviders);
  }

  @Test
  public void testAbortedTraceCallAbortByID() throws IOException {
    final long traceId = 0xFACEB00C;
    mOrchestrator.onTraceWriteStart(traceId, 0);
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    verify(mTraceControl)
        .cleanupTraceContextByID(
            eq(traceId), eq(ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED));
  }

  @Test
  public void testAbortedNonexistentTraceCallAbortByID() throws IOException {
    /* This can happen if we never saw the trace start event and thus opened a real file. */
    final long traceId = 0xFACEB00C;
    mOrchestrator.onTraceWriteStart(traceId, 0);
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    verify(mTraceControl)
        .cleanupTraceContextByID(
            eq(traceId), eq(ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED));
  }

  @Test
  public void testTraceUploadOnTimeout() throws Exception {
    mOrchestrator.setConfigProvider(new TestConfigProvider(buildConfigWithTimedOutSampleRate(1)));

    File folder = Files.newTemporaryFolder();
    File fileA = File.createTempFile("tracetmp", "tmp", folder);
    File fileB = File.createTempFile("tracetmp", "tmp", folder);

    final long traceId = mTraceContext.traceId;
    mTraceContext.abortReason = ProfiloConstants.ABORT_REASON_TIMEOUT;
    mTraceContext.folder = folder;

    try {
      mOrchestrator.onTraceStartSync(mTraceContext);
      mOrchestrator.onTraceStartAsync(mTraceContext);
      mOrchestrator.onTraceAbort(mTraceContext);

      mOrchestrator.onTraceWriteStart(traceId, 0);
      mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_TIMEOUT);
    } finally {
      fileA.delete();
      fileB.delete();
      folder.delete();
    }

    verify(mFileManager).addFileToUploads(any(File.class), anyBoolean());
  }

  @Test
  public void testTraceIsDiscardedOnTimeout() throws Exception {
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    mOrchestrator.setConfigProvider(new TestConfigProvider(buildConfigWithTimedOutSampleRate(0)));
    final long traceId = 0xFACEB00C;
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mOrchestrator.onTraceWriteStart(traceId, 0);
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    verify(mFileManager, never()).addFileToUploads(any(File.class), anyBoolean());
  }

  @Test
  public void testTraceMemoryOnlyTraceAddedToUploadsAsNotTrimmable() throws Exception {
    mOrchestrator.setConfigProvider(new TestConfigProvider(buildConfigWithTimedOutSampleRate(1)));
    final long traceId = mTraceContext.traceId;
    mTraceContext.flags = Trace.FLAG_MEMORY_ONLY;

    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mTraceContext.folder = tempDirectory;
    try {
      mOrchestrator.onTraceStartSync(mTraceContext);
      mOrchestrator.onTraceStartAsync(mTraceContext);
      mOrchestrator.onTraceWriteStart(traceId, Trace.FLAG_MEMORY_ONLY);
      mOrchestrator.onTraceStop(mTraceContext);
      mOrchestrator.onTraceWriteEnd(traceId);

      verify(mFileManager).addFileToUploads(any(File.class), eq(false));
    } finally {
      traceFile.delete();
      tempDirectory.delete();
    }
  }

  @Test
  public void testWriterCallbacksLifecycle() throws IOException {
    final long traceId = mTraceContext.traceId;
    File folder = Files.newTemporaryFolder();
    File fileA = File.createTempFile("tracetmp", "tmp", folder);
    mTraceContext.folder = folder;

    try {
      mOrchestrator.onTraceStartSync(mTraceContext);
      mOrchestrator.onTraceStartAsync(mTraceContext);
      mOrchestrator.onTraceStop(mTraceContext);

      mOrchestrator.onTraceWriteStart(traceId, 0);
      mOrchestrator.onTraceWriteEnd(traceId);

      mTraceContext.traceId += 1;
      mOrchestrator.onTraceStartSync(mTraceContext);
      mOrchestrator.onTraceStartAsync(mTraceContext);
      mOrchestrator.onTraceStop(mTraceContext);
      mOrchestrator.onTraceWriteStart(mTraceContext.traceId, 0);
      // Assert that onTraceWriteStart doesn't throw
    } finally {
      fileA.delete();
      folder.delete();
    }
  }

  @Test
  public void testWriterCallbacksLifecycleInSecondProcess() throws IOException {
    Whitebox.setInternalState(mOrchestrator, "mIsMainProcess", false);
    testWriterCallbacksLifecycle();
  }

  @Test
  public void testBaseTraceProviderChanges() {
    assertThatAllProvidersDisabled();

    // First we start a trace that enables both Base and Sync provider.
    // Then we start a trace that enables only Sync.
    // Then we stop the first trace and expect Base to be restarted for a new provider mask.
    // The we stop the second trace and expect both to be stopped.

    mOrchestrator.onTraceStartSync(mTraceContext);
    InOrder syncOrder = inOrder(mSyncBaseTraceProvider);
    syncOrder.verify(mSyncBaseTraceProvider).enable();

    mOrchestrator.onTraceStartAsync(mTraceContext);
    InOrder baseOrder = inOrder(mBaseTraceProvider);
    baseOrder.verify(mBaseTraceProvider).enable();

    mOrchestrator.onTraceStartSync(mSecondTraceContext);
    mOrchestrator.onTraceStartAsync(mSecondTraceContext);

    mOrchestrator.onTraceStop(mTraceContext);
    baseOrder.verify(mBaseTraceProvider).disable();
    baseOrder.verify(mBaseTraceProvider).enable();
    mOrchestrator.onTraceStop(mSecondTraceContext);

    baseOrder.verify(mBaseTraceProvider).disable();
    baseOrder.verify(mBaseTraceProvider, never()).enable();
    baseOrder.verify(mBaseTraceProvider, never()).disable();

    syncOrder.verify(mSyncBaseTraceProvider).disable();
    syncOrder.verify(mSyncBaseTraceProvider, never()).enable();
    syncOrder.verify(mSyncBaseTraceProvider, never()).disable();
  }

  @Test
  public void testBaseTraceProviderSimple() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStartSync(mTraceContext);
    verify(mSyncBaseTraceProvider).enable();
    mOrchestrator.onTraceStartAsync(mTraceContext);
    verify(mBaseTraceProvider).enable();
    mOrchestrator.onTraceStop(mTraceContext);
    verify(mBaseTraceProvider).disable();
    verify(mSyncBaseTraceProvider).disable();
  }

  @Test
  public void testBaseTraceProvidersNotChanging() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStartSync(mTraceContext);
    verify(mSyncBaseTraceProvider).enable();
    mOrchestrator.onTraceStartAsync(mTraceContext);
    verify(mBaseTraceProvider).enable();
    Buffer buffer = mTraceContextRule.newBuffer();
    TraceContext anotherContext =
        new TraceContext(
            0xFACEB001, // traceId
            "FACEBOOO1", // encodedTraceId
            mConfigProvider.getFullConfig(), // config
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            DEFAULT_TRACING_PROVIDERS,
            1, // flags
            1,
            TraceConfigExtras.EMPTY,
            buffer,
            new Buffer[] {buffer},
            new File("."),
            "prefix-"); // mTraceConfigExtras
    mOrchestrator.onTraceStartSync(anotherContext);
    mOrchestrator.onTraceStartAsync(anotherContext);
    verify(mBaseTraceProvider, never()).disable();
    verify(mSyncBaseTraceProvider, never()).disable();
    mOrchestrator.onTraceStop(mTraceContext);
    verify(mBaseTraceProvider, never()).disable();
    verify(mSyncBaseTraceProvider, never()).disable();
    mOrchestrator.onTraceStop(anotherContext);
    verify(mBaseTraceProvider).disable();
    verify(mSyncBaseTraceProvider).disable();
  }

  @Test
  public void testTracingProvidersOnProvidersStop() {
    TraceOrchestratorListener listener = mock(TraceOrchestratorListener.class);
    mOrchestrator.addListener(listener);

    int enabledProvidersMask = 0x11111;
    Buffer buffer = mTraceContextRule.newBuffer();
    TraceContext traceContext =
        new TraceContext(
            0xFACEB000, // traceId
            "FACEBOOO0", // encodedTraceId
            mConfigProvider.getFullConfig(), // config
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            enabledProvidersMask, // enabled providers mask
            1,
            0,
            TraceConfigExtras.EMPTY,
            /*buffer*/ buffer,
            new Buffer[] {buffer},
            new File("."),
            "prefix-"); // mTraceConfigExtras
    mOrchestrator.onTraceStop(traceContext);

    ArgumentCaptor<Integer> providerMaskCaptor = ArgumentCaptor.forClass(Integer.class);
    verify(listener).onProvidersStop(same(traceContext), providerMaskCaptor.capture());
    int tracingProviders = providerMaskCaptor.getValue();
    assertThat(tracingProviders)
        .isEqualTo(
            mBaseTraceProvider.getTracingProviders()
                | mSyncBaseTraceProvider.getTracingProviders());
  }

  @Test
  public void testTracingConstantsProvidersOnProvidersStop() {
    TraceOrchestratorListener listener = mock(TraceOrchestratorListener.class);
    mOrchestrator.addListener(listener);

    final int enabledProvidersMask = 0x1011000;
    mOrchestrator.addTraceProvider(
        new BaseTraceProvider() {
          @Override
          protected void enable() {}

          @Override
          protected void disable() {}

          @Override
          protected int getSupportedProviders() {
            return enabledProvidersMask;
          }

          @Override
          protected int getTracingProviders() {
            return TraceEvents.enabledMask(enabledProvidersMask);
          }
        });

    TraceEvents.enableProviders(enabledProvidersMask);
    Buffer buffer = mTraceContextRule.newBuffer();
    TraceContext traceContext =
        new TraceContext(
            0xFACEB000, // traceId
            "FACEBOOO0", // encodedTraceId
            mConfigProvider.getFullConfig(), // config
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            enabledProvidersMask, // enabled providers mask
            1,
            0,
            TraceConfigExtras.EMPTY,
            buffer,
            new Buffer[] {buffer},
            new File("."),
            "prefix-"); // mTraceConfigExtras
    mOrchestrator.onTraceStop(traceContext);

    ArgumentCaptor<Integer> providerMaskCaptor = ArgumentCaptor.forClass(Integer.class);
    verify(listener).onProvidersStop(same(traceContext), providerMaskCaptor.capture());
    int tracingProviders = providerMaskCaptor.getValue();
    assertThat(tracingProviders & enabledProvidersMask).isEqualTo(enabledProvidersMask);
  }

  private static void verifyProvidersDisabled(BaseTraceProvider... providers) {
    for (BaseTraceProvider provider : providers) {
      verify(provider)
          .onDisable(any(TraceContext.class), any(BaseTraceProvider.ExtraDataFileProvider.class));
    }
  }

  private void assertProvidersTheSame(int providers) {
    assertThat(TraceEvents.enabledMask(providers)).isEqualTo(providers);
  }

  private void assertThatAllProvidersDisabled() {
    assertThat(TraceEvents.enabledMask(0xFFFFFFFF)).isZero();
  }
}
