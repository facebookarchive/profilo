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
import android.os.HandlerThread;
import android.os.Message;
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
import org.mockito.internal.util.reflection.Whitebox;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.core.classloader.annotations.SuppressStaticInitializationFor;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
@PrepareForTest({
  Context.class,
  FileManager.class,
  Log.class,
  Logger.class,
  Message.class,
  TraceControl.class,
  TraceOrchestrator.class,
  TraceEvents.class,
  SoLoader.class,
  SparseArray.class,
  Process.class,
})
@SuppressStaticInitializationFor({
  "com.facebook.profilo.logger.Logger",
  "com.facebook.profilo.core.TraceEvents"
})
public class TraceOrchestratorTest {

  // These values are the inverse of each other, so we can do just one TraceEvents.isEnabled call
  private static final int DEFAULT_TRACING_PROVIDERS = 0xffffffff;
  private static final int SECOND_TRACE_TRACING_PROVIDERS = 0x11011;
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
  private TraceOrchestrator.TraceProvider mTraceProvider;
  private BaseTraceProvider mBaseTraceProvider;

  static class TestBaseProvider extends BaseTraceProvider {

    @Override
    protected void enable() {}

    @Override
    protected void disable() {}

    @Override
    protected int getSupportedProviders() {
      return 0x101;
    }
  }

  @Before
  public void setUp() throws Exception {
    mockStatic(
        Log.class,
        Logger.class,
        TraceControl.class,
        Process.class);

    mTraceControl = mock(TraceControl.class);
    when(TraceControl.get()).thenReturn(mTraceControl);

    mockStatic(SoLoader.class);
    SoLoader.setInTestMode();

    mockStatic(Process.class);
    when(Process.myPid()).thenReturn(DEFAULT_PROCESS_ID);

    whenNew(HandlerThread.class).withAnyArguments().thenReturn(mock(HandlerThread.class));
    whenNew(TraceOrchestrator.TraceEventsHandler.class)
        .withAnyArguments()
        .thenReturn(mock(TraceOrchestrator.TraceEventsHandler.class));

    mContext = mock(Context.class);
    mFileManager = mock(FileManager.class);
    mUploadService = mock(BackgroundUploadService.class);
    mConfigProvider = new TestConfigProvider();
    mTraceContext =
        new TraceContext(
            0xFACEB00C, // traceId
            "FACEBOOOK", // encodedTraceId
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            DEFAULT_TRACING_PROVIDERS,
            0, // cpuSamplingRateMs
            1); // flags
    mSecondTraceContext =
        new TraceContext(
            0xFACEB000, // traceId
            "FACEBOOO0", // encodedTraceId
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            SECOND_TRACE_TRACING_PROVIDERS,
            0, // cpuSamplingRateMs
            1); // flags

    whenNew(SparseArray.class).withAnyArguments()
        .thenReturn(mock(SparseArray.class, RETURNS_MOCKS));
    whenNew(FileManager.class).withAnyArguments().thenReturn(mFileManager);
    mTraceProvider = mock(TraceOrchestrator.TraceProvider.class);
    mBaseTraceProvider = spy(new TestBaseProvider());
    when(mFileManager.getAndResetStatistics()).
        thenReturn(mock(FileManager.FileManagerStatistics.class));
    mOrchestrator =
        new TraceOrchestrator(
            mContext,
            mConfigProvider,
            new TraceOrchestrator.TraceProvider[] {mTraceProvider, mBaseTraceProvider},
            true); // isMainProcess
    mOrchestrator.bind(
        mContext,
        new SparseArray<TraceController>(1),
        TraceOrchestrator.MAIN_PROCESS_NAME);

    whenNew(Message.class).withAnyArguments().thenReturn(mock(Message.class));
    whenNew(TraceOrchestrator.TraceEventsHandler.class).withAnyArguments().thenReturn(
        mock(TraceOrchestrator.TraceEventsHandler.class));
  }

  @Test
  public void testSetBackgroundUploadServiceTriggersImmediateUpload() {
    List<File> tracesList = new ArrayList<>();
    List<File> tracesListSkipChecks = new ArrayList<>();

    when(mFileManager.getTrimmableFilesToUpload()).thenReturn(tracesList);
    when(mFileManager.getUntrimmableFilesToUpload()).thenReturn(tracesListSkipChecks);

    mOrchestrator.setBackgroundUploadService(mUploadService);
    verify(mUploadService).uploadInBackground(
        same(tracesList),
        any(BackgroundUploadService.BackgroundUploadListener.class));

    verify(mUploadService).uploadInBackgroundSkipChecks(
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
  public void testConfigChangeDuringTraceIsDeferredToStop() {
    when(mTraceControl.isInsideTrace()).thenReturn(true); // Inside trace

    mOrchestrator.onTraceStart(mTraceContext);
    mOrchestrator.onConfigUpdated(mConfigProvider.getFullConfig());
    // Providers are untouched
    assertProvidersTheSame(mTraceContext.enabledProviders);
    // TraceControl doesn't get called with new config
    verify(mTraceControl, times(1)).setConfig(any(Config.class));

    // Stop Trace to see the deferred change
    when(mTraceControl.isInsideTrace()).thenReturn(false);
    mOrchestrator.onTraceStop(mTraceContext);

    assertThatAllProvidersDisabled();
    verify(mTraceControl, times(2)).setConfig(any(Config.class));
  }

  @Test
  public void testConfigChangeDuringDeferredForMultipleRunningTraces() {
    when(mTraceControl.isInsideTrace()).thenReturn(true); // Inside trace

    mOrchestrator.onTraceStart(mTraceContext);
    mOrchestrator.onConfigUpdated(mConfigProvider.getFullConfig());
    mOrchestrator.onTraceStart(mSecondTraceContext);

    // Providers are untouched
    assertProvidersTheSame(mTraceContext.enabledProviders | mSecondTraceContext.enabledProviders);
    // TraceControl doesn't get called with new config
    verify(mTraceControl, times(1)).setConfig(any(Config.class));

    // First trace is stopped but the second one is still running, expect no config change
    mOrchestrator.onTraceStop(mSecondTraceContext);
    assertProvidersTheSame(mTraceContext.enabledProviders);
    verify(mTraceControl, times(1)).setConfig(any(Config.class));

    // Stop Trace to see the deferred change
    when(mTraceControl.isInsideTrace()).thenReturn(false);
    mOrchestrator.onTraceStop(mTraceContext);

    assertThatAllProvidersDisabled();
    verify(mTraceControl, times(2)).setConfig(any(Config.class));
  }

  @Test
  public void testTracingChangesProviders() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStart(mTraceContext);
    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isTrue();
  }

  @Test
  public void testMultiTracingChangesProviders() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStart(mTraceContext);
    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isTrue();
    mOrchestrator.onTraceStart(mSecondTraceContext);
    mOrchestrator.onTraceStop(mTraceContext);
    assertThat(TraceEvents.isEnabled(SECOND_TRACE_TRACING_PROVIDERS)).isTrue();
    mOrchestrator.onTraceStop(mSecondTraceContext);
    assertThatAllProvidersDisabled();
  }

  @Test
  public void testTraceStopChangesProviders() {
    mOrchestrator.onTraceStart(mTraceContext);
    mOrchestrator.onTraceStop(mTraceContext);

    verifyProvidersDisabled();
    assertThatAllProvidersDisabled();
  }

  @Test
  public void testTraceAbortChangesProviders() {
    mOrchestrator.onTraceStart(mTraceContext);
    assertThat(TraceEvents.isEnabled(DEFAULT_TRACING_PROVIDERS)).isTrue();
    TraceContext traceContext =
        new TraceContext(mTraceContext, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);
    mOrchestrator.onTraceAbort(traceContext);
    assertThatAllProvidersDisabled();
    verifyProvidersDisabled();
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
    mOrchestrator.onTraceWriteEnd(traceId, 0);
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
    mOrchestrator.onTraceWriteEnd(traceId, 0);
    mOrchestrator.onTraceWriteStart(traceId + 1, 0, "this-file-does-not-exist-profilo-test");
    // Assert that onTraceWriteStart doesn't throw
  }

  @Test
  public void testBaseTraceProviderChanges() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStart(mTraceContext);
    mOrchestrator.traceStart(mTraceContext);
    verify(mBaseTraceProvider).enable();
    mOrchestrator.onTraceStart(mSecondTraceContext);
    mOrchestrator.traceStart(mSecondTraceContext);
    verify(mBaseTraceProvider, times(1)).enable();
    mOrchestrator.onTraceStop(mTraceContext);
    verify(mBaseTraceProvider, times(1)).disable();
    verify(mBaseTraceProvider, times(2)).enable();
    mOrchestrator.onTraceStop(mSecondTraceContext);
    verify(mBaseTraceProvider, times(2)).disable();
  }

  @Test
  public void testBaseTraceProviderSimple() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStart(mTraceContext);
    mOrchestrator.traceStart(mTraceContext);
    verify(mBaseTraceProvider).enable();
    mOrchestrator.onTraceStop(mTraceContext);
    verify(mBaseTraceProvider).disable();
  }

  @Test
  public void testBaseTraceProvidersNotChanging() {
    assertThatAllProvidersDisabled();
    mOrchestrator.onTraceStart(mTraceContext);
    mOrchestrator.traceStart(mTraceContext);
    verify(mBaseTraceProvider).enable();
    TraceContext anotherContext =
        new TraceContext(
            0xFACEB001, // traceId
            "FACEBOOO1", // encodedTraceId
            0, // controller
            null, // controllerObject
            null, // context
            0, // intContext
            DEFAULT_TRACING_PROVIDERS,
            0, // cpuSamplingRateMs
            1, // flags
            1); // configId
    mOrchestrator.onTraceStart(anotherContext);
    mOrchestrator.traceStart(anotherContext);
    verify(mBaseTraceProvider, never()).disable();
    mOrchestrator.onTraceStop(mTraceContext);
    verify(mBaseTraceProvider, never()).disable();
    mOrchestrator.onTraceStop(anotherContext);
    verify(mBaseTraceProvider).disable();
  }

  @Test
  public void testTraceFileChecksumRename() throws IOException {
    final long traceId = 0xFACEB00C;
    final int crc = 0xC00BECAF;
    File tempDirectory = new File(DEFAULT_TEMP_DIR);
    tempDirectory.mkdirs();
    File traceFile = File.createTempFile("tmp", "tmp.log", tempDirectory);
    TraceOrchestrator.TraceListener fileListener = mock(TraceOrchestrator.TraceListener.class);
    mOrchestrator.addListener(fileListener);

    mOrchestrator.onTraceWriteStart(traceId, 0, traceFile.getPath());
    mOrchestrator.onTraceWriteEnd(traceId, crc);

    ArgumentCaptor<File> fileCaptor = ArgumentCaptor.forClass(File.class);
    verify(fileListener).onTraceFlushed(fileCaptor.capture(), eq(traceId));
    String filename = fileCaptor.getValue().getName();
    String filenameNoExt = filename.substring(0, filename.lastIndexOf('.'));
    String result_crc =
        filenameNoExt.substring(
            filenameNoExt.lastIndexOf(TraceOrchestrator.CHECKSUM_DELIM)
                + TraceOrchestrator.CHECKSUM_DELIM.length());
    assertThat(result_crc).isEqualTo(Integer.toHexString(crc));
  }

  private void verifyProvidersDisabled() {
    verify(mTraceProvider).onDisable(any(TraceContext.class), any(File.class));
  }

  private void assertProvidersTheSame(int providers) {
    assertThat(TraceEvents.enabledMask(providers)).isEqualTo(providers);
  }

  private void assertThatAllProvidersDisabled() {
    assertThat(TraceEvents.enabledMask(0xFFFFFFFF)).isZero();
  }
}
