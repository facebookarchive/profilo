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
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.eq;
import static org.mockito.Matchers.notNull;
import static org.mockito.Matchers.same;
import static org.mockito.Mockito.RETURNS_MOCKS;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.spy;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;
import static org.powermock.api.mockito.PowerMockito.when;
import static org.powermock.api.mockito.PowerMockito.whenNew;

import android.content.Context;
import android.os.Process;
import android.util.Log;
import android.util.SparseArray;
import com.facebook.profilo.config.Config;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.FileManager;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.util.TestConfigProvider;
import com.facebook.profilo.util.TraceEventsFakeRule;
import com.facebook.soloader.SoLoader;
import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.core.classloader.annotations.SuppressStaticInitializationFor;
import org.powermock.modules.junit4.PowerMockRunner;
import org.powermock.reflect.Whitebox;

@RunWith(PowerMockRunner.class)
@PrepareForTest({
  Context.class,
  FileManager.class,
  Log.class,
  Logger.class,
  TraceControl.class,
  TraceOrchestrator.class,
  TraceEvents.class,
  SoLoader.class,
  SparseArray.class,
  Process.class,
  BaseTraceProvider.class,
})
@SuppressStaticInitializationFor({
  "com.facebook.profilo.logger.Logger",
  "com.facebook.profilo.core.TraceEvents"
})
public class TraceOrchestratorTest {

  // These values are the inverse of each other, so we can do just one TraceEvents.isEnabled call
  private static final int DEFAULT_TRACING_PROVIDERS = 0xffffffff;
  private static final int SECOND_TRACE_TRACING_PROVIDERS = 0x11011;
  private static final int NORMAL_PROVIDER_SUPPORTED_PROVIDERS = 0x101;
  private static final int NORMAL_PROVIDER_TRACING_PROVIDERS = 0x001;
  private static final int SYNC_PROVIDER_SUPPORTED_PROVIDERS = 0x10;
  private static final int SYNC_PROVIDER_TRACING_PROVIDERS = 0x10;
  private static final int DEFAULT_PROCESS_ID = 1111;
  private static final String DEFAULT_TEMP_DIR = "temp_dir";

  private BackgroundUploadService mUploadService;
  private Context mContext;
  private FileManager mFileManager;
  private TestConfigProvider mConfigProvider;
  private TraceOrchestrator mOrchestrator;
  private TraceControl mTraceControl;
  private TraceContext mTraceContext;
  private TraceContext mSecondTraceContext;

  @Rule TraceEventsFakeRule mTraceEventsRule = new TraceEventsFakeRule();
  private BaseTraceProvider mTraceProvider;
  private BaseTraceProvider mBaseTraceProvider;
  private BaseTraceProvider mSyncBaseTraceProvider;

  private BaseTraceProvider[] mTraceProviders;

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

  @Before
  public void setUp() throws Exception {
    mockStatic(Log.class, Logger.class, TraceControl.class, Process.class);

    mTraceControl = mock(TraceControl.class);
    when(TraceControl.get()).thenReturn(mTraceControl);

    mockStatic(SoLoader.class);
    SoLoader.setInTestMode();

    mockStatic(Process.class);
    when(Process.myPid()).thenReturn(DEFAULT_PROCESS_ID);

    mContext = mock(Context.class);
    mFileManager = mock(FileManager.class);
    mUploadService = mock(BackgroundUploadService.class);
    mConfigProvider = new TestConfigProvider();
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
            TraceContext.TraceConfigExtras.EMPTY); // mTraceConfigExtras
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
            TraceContext.TraceConfigExtras.EMPTY); // mTraceConfigExtras

    whenNew(SparseArray.class)
        .withAnyArguments()
        .thenReturn(mock(SparseArray.class, RETURNS_MOCKS));
    whenNew(FileManager.class).withAnyArguments().thenReturn(mFileManager);
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
    mOrchestrator =
        new TraceOrchestrator(
            mContext,
            mConfigProvider,
            mTraceProviders,
            TraceOrchestrator.MAIN_PROCESS_NAME,
            true, // isMainProcess
            null); // Default trace location
    mOrchestrator.bind(mContext, new SparseArray<TraceController>(1));
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
            same(tracesList), any(BackgroundUploadService.BackgroundUploadListener.class));

    verify(mUploadService, times(1))
        .uploadInBackgroundSkipChecks(
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
            same(tracesList), any(BackgroundUploadService.BackgroundUploadListener.class));

    verify(mUploadService, times(1))
        .uploadInBackgroundSkipChecks(
            same(tracesListSkipChecks),
            any(BackgroundUploadService.BackgroundUploadListener.class));
  }

  @Test
  public void testInitialBindInstallsProviders() {
    verifyStatic();
    //noinspection unchecked
    TraceControl.initialize(notNull(SparseArray.class), same(mOrchestrator), notNull(Config.class));

    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isFalse();
  }

  @Test
  public void testConfigChangeInstallsNewConfig() {
    reset(mTraceControl);
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
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mOrchestrator.onTraceWriteStart(traceId, 0, traceFile.getPath());
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    verify(mTraceControl)
        .cleanupTraceContextByID(
            eq(traceId), eq(ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED));
  }

  @Test
  public void testAbortedNonexistentTraceCallAbortByID() throws IOException {
    /* This can happen if we never saw the trace start event and thus opened a real file. */
    final long traceId = 0xFACEB00C;
    mOrchestrator.onTraceWriteStart(traceId, 0, "this-file-does-not-exist-profilo-test");
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    verify(mTraceControl)
        .cleanupTraceContextByID(
            eq(traceId), eq(ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED));
  }

  @Test
  public void testTraceUploadOnTimeout() throws Exception {
    mConfigProvider.setTimedOutUploadSampleRate(1);
    final long traceId = 0xFACEB00C;
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mOrchestrator.onTraceWriteStart(traceId, 0, traceFile.getPath());
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_TIMEOUT);

    verify(mFileManager).addFileToUploads(any(File.class), anyBoolean());
  }

  @Test
  public void testTraceIsDiscardedOnTimeout() throws Exception {
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    mConfigProvider.setTimedOutUploadSampleRate(0);
    final long traceId = 0xFACEB00C;
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mOrchestrator.onTraceWriteStart(traceId, 0, traceFile.getPath());
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    verify(mFileManager, never()).addFileToUploads(any(File.class), anyBoolean());
  }

  @Test
  public void testWriterCallbacksLifecycle() {
    final long traceId = 0xFACEB00C;
    mOrchestrator.onTraceWriteStart(traceId, 0, "this-file-does-not-exist-profilo-test");
    mOrchestrator.onTraceWriteEnd(traceId);
    mOrchestrator.onTraceWriteStart(traceId + 1, 0, "this-file-does-not-exist-profilo-test");
    // Assert that onTraceWriteStart doesn't throw
  }

  @Test
  public void testWriterCallbacksLifecycleOnTimeout() throws IOException {
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    mConfigProvider.setTimedOutUploadSampleRate(0);
    final long traceId = 0xFACEB00C;
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mOrchestrator.onTraceWriteStart(traceId, 0, traceFile.getPath());
    mOrchestrator.onTraceWriteAbort(traceId, ProfiloConstants.ABORT_REASON_TIMEOUT);
    mOrchestrator.onTraceWriteStart(traceId + 1, 0, "this-file-does-not-exist-profilo-test");
    // Assert that onTraceWriteStart doesn't throw
  }

  @Test
  public void testWriterCallbacksLifecycleInSecondProcess() throws IOException {
    Whitebox.setInternalState(mOrchestrator, "mIsMainProcess", false);
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    mConfigProvider.setTimedOutUploadSampleRate(0);
    final long traceId = 0xFACEB00C;
    File traceFile = File.createTempFile("tmp", "tmp", tempDirectory);
    mOrchestrator.onTraceWriteStart(traceId, 0, traceFile.getPath());
    mOrchestrator.onTraceWriteEnd(traceId);
    mOrchestrator.onTraceWriteStart(traceId + 1, 0, "this-file-does-not-exist-profilo-test");
    // Assert that onTraceWriteStart doesn't throw
  }

  @Test
  public void testBaseTraceProviderChanges() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStartSync(mTraceContext);
    verify(mSyncBaseTraceProvider).enable();
    mOrchestrator.onTraceStartAsync(mTraceContext);
    verify(mBaseTraceProvider).enable();
    mOrchestrator.onTraceStartSync(mSecondTraceContext);
    verify(mSyncBaseTraceProvider, times(1)).enable();
    mOrchestrator.onTraceStartAsync(mSecondTraceContext);
    verify(mBaseTraceProvider, times(1)).enable();
    mOrchestrator.onTraceStop(mTraceContext);
    verify(mBaseTraceProvider, times(1)).disable();
    verify(mBaseTraceProvider, times(2)).enable();
    mOrchestrator.onTraceStop(mSecondTraceContext);
    verify(mBaseTraceProvider, times(2)).disable();
    verify(mSyncBaseTraceProvider, times(1)).disable();
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
            1, // configId
            TraceContext.TraceConfigExtras.EMPTY); // mTraceConfigExtras
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
            TraceContext.TraceConfigExtras.EMPTY); // mTraceConfigExtras
    mOrchestrator.onTraceStop(traceContext);

    ArgumentCaptor<Integer> providerMaskCaptor = ArgumentCaptor.forClass(Integer.class);
    verify(listener).onProvidersStop(providerMaskCaptor.capture());
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
            TraceContext.TraceConfigExtras.EMPTY); // mTraceConfigExtras
    mOrchestrator.onTraceStop(traceContext);

    ArgumentCaptor<Integer> providerMaskCaptor = ArgumentCaptor.forClass(Integer.class);
    verify(listener).onProvidersStop(providerMaskCaptor.capture());
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
