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
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.same;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;
import static org.powermock.api.mockito.PowerMockito.when;

import android.util.SparseArray;
import com.facebook.fbtrace.utils.FbTraceId;
import com.facebook.profilo.config.ConfigImpl;
import com.facebook.profilo.config.ConfigParams;
import com.facebook.profilo.config.ConfigTraceConfig;
import com.facebook.profilo.entries.EntryType;
import com.facebook.profilo.ipc.TraceConfigExtras;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.BufferLogger;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.mmapbuf.core.Buffer;
import com.facebook.profilo.mmapbuf.core.MmapBufferManager;
import com.facebook.profilo.util.TestConfigProvider;
import com.facebook.profilo.writer.NativeTraceWriter;
import com.facebook.testing.powermock.PowerMockTest;
import com.facebook.testing.robolectric.v4.WithTestDefaultsRunner;
import java.io.File;
import java.util.TreeMap;
import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.core.classloader.annotations.SuppressStaticInitializationFor;
import org.powermock.reflect.Whitebox;

@RunWith(WithTestDefaultsRunner.class)
@PrepareForTest({
  Logger.class,
  TraceControl.class,
  ProvidersRegistry.class,
})
@SuppressStaticInitializationFor({
  "com.facebook.profilo.logger.BufferLogger",
  "com.facebook.profilo.mmapbuf.Buffer",
  "com.facebook.profilo.mmapbuf.MmapBufferManager",
})
@Ignore
public class TraceControlTest extends PowerMockTest {

  private static final int TRACE_CONTROLLER_ID = 100;
  private static final long TEST_TRACE_ID = 10000l;

  private static final int PROVIDER_TEST = ProvidersRegistry.newProvider("test");
  private static final String TEST_INT_PARAM_KEY = "TEST_KEY";
  private static final int TEST_INT_PARAM_VALUE = 0x100;

  private TraceControl mTraceControl;
  private SparseArray<TraceController> mControllers;
  private TraceController mNonconfigurableController;
  private TraceController mConfigurableController;
  private TraceControlHandler mTraceControlHandler;
  private TraceContext mTraceContext;
  private final ConfigTraceConfig mTraceConfig = new ConfigTraceConfig();
  private final ConfigImpl mConfig = new ConfigImpl(0, new ConfigParams(), mTraceConfig);
  private Buffer mBuffer;

  @Before
  public void setUp() throws Exception {
    mockStatic(BufferLogger.class);
    mockStatic(ProvidersRegistry.class);

    mNonconfigurableController = mock(TraceController.class);

    when(mNonconfigurableController.isConfigurable()).thenReturn(false);

    // Register both null and non-null context param
    when(mNonconfigurableController.getNonConfigurableProviders(anyLong(), any(Object.class)))
        .thenReturn(PROVIDER_TEST);
    when(mNonconfigurableController.getNonConfigurableProviders(anyLong(), isNull(Object.class)))
        .thenReturn(PROVIDER_TEST);

    when(mNonconfigurableController.contextsEqual(
            anyLong(), any(Object.class), anyLong(), any(Object.class)))
        .thenReturn(true);
    when(mNonconfigurableController.contextsEqual(anyLong(), isNull(), anyLong(), isNull()))
        .thenReturn(true);

    when(mNonconfigurableController.getNonConfigurableTraceConfigExtras(
            anyLong(), any(Object.class)))
        .thenReturn(TraceConfigExtras.EMPTY);
    when(mNonconfigurableController.getNonConfigurableTraceConfigExtras(
            anyLong(), isNull(Object.class)))
        .thenReturn(TraceConfigExtras.EMPTY);

    mConfigurableController = mock(TraceController.class);
    when(mConfigurableController.isConfigurable()).thenReturn(true);
    when(mConfigurableController.contextsEqual(
            anyLong(), any(Object.class), anyLong(), any(Object.class)))
        .thenReturn(true);
    when(mConfigurableController.contextsEqual(anyLong(), isNull(), anyLong(), isNull()))
        .thenReturn(true);

    mTraceConfig.params = new ConfigParams();
    mTraceConfig.params.intParams = new TreeMap<>();
    mTraceConfig.params.intParams.put(TEST_INT_PARAM_KEY, TEST_INT_PARAM_VALUE);
    mTraceConfig.enabledProviders = new String[] {"test_provider"};

    when(ProvidersRegistry.getBitMaskFor(anyString())).thenReturn(1);
    when(ProvidersRegistry.getBitMaskFor(any(Iterable.class))).thenReturn(1);

    mControllers = mock(SparseArray.class);
    setController(mNonconfigurableController);

    MmapBufferManager manager = mock(MmapBufferManager.class);
    mBuffer = mock(Buffer.class);
    when(manager.allocateBuffer(anyInt(), anyBoolean())).thenReturn(mBuffer);
    mTraceControl =
        new TraceControl(
            mControllers,
            mConfig,
            mock(TraceControl.TraceControlListener.class),
            manager,
            new File("."),
            "process",
            null);

    mTraceControlHandler = mock(TraceControlHandler.class);
    Whitebox.setInternalState(mTraceControl, "mTraceControlHandler", mTraceControlHandler);

    mTraceContext =
        new TraceContext(
            TEST_TRACE_ID,
            FbTraceId.encode(TEST_TRACE_ID),
            mConfig,
            1111,
            new Object(),
            new Object(),
            1,
            PROVIDER_TEST,
            1,
            222,
            TraceConfigExtras.EMPTY,
            /*buffer*/ null,
            new Buffer[] {},
            new File("."),
            "prefix-");
  }

  private void setController(TraceController controller) {
    when(mControllers.get(eq(TRACE_CONTROLLER_ID))).thenReturn(controller);
    when(mControllers.get(eq(TRACE_CONTROLLER_ID), any(TraceController.class)))
        .thenReturn(controller);
  }

  @Test
  public void testStartWithExistingTraceContext() {
    // Context from IPC is absent
    mTraceContext.config = null;
    assertThat(mTraceControl.adoptContext(TRACE_CONTROLLER_ID, 0, mTraceContext)).isTrue();
    assertTracing();

    TraceContext currContext = mTraceControl.getCurrentTraces().get(0);
    assertThat(currContext).isNotNull();
    assertThat(currContext.traceId).isEqualTo(mTraceContext.traceId);
    assertThat(currContext.encodedTraceId).isEqualTo(mTraceContext.encodedTraceId);
    assertThat(currContext.controller).isEqualTo(TRACE_CONTROLLER_ID);
    assertThat(currContext.controllerObject).isEqualTo(mNonconfigurableController);
    assertThat(currContext.context).isEqualTo(mTraceContext.context);
    assertThat(currContext.longContext).isEqualTo(mTraceContext.longContext);
    assertThat(currContext.enabledProviders).isEqualTo(mTraceContext.enabledProviders);
    assertThat(currContext.mTraceConfigExtras).isEqualTo(mTraceContext.mTraceConfigExtras);
    assertThat(currContext.config).isNotNull();
  }

  @Test
  public void testStartFiltersOutControllers() {
    TraceController secondController = mock(TraceController.class);
    when(secondController.getNonConfigurableProviders(anyLong(), anyObject())).thenReturn(0);
    when(secondController.contextsEqual(anyInt(), anyObject(), anyInt(), anyObject()))
        .thenReturn(true);
    when(secondController.isConfigurable()).thenReturn(false);
    when(mControllers.get(eq(~TRACE_CONTROLLER_ID))).thenReturn(secondController);

    assertThat(mTraceControl.startTrace(~TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertNotTracing();

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
    /*
    assertTracing();
    */
  }

  @Test(expected = RuntimeException.class)
  public void testThrowsOnConfigWithoutController() {
    when(mControllers.get(eq(TRACE_CONTROLLER_ID))).thenReturn(null);
    mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0);
    fail("TraceControl did not throw when querying for unregistered controllerID");
  }

  @Test
  public void testNonConfigurableControllerTraceStart() {
    TraceController noConfController = mock(TraceController.class);
    when(noConfController.getNonConfigurableProviders(anyLong(), anyObject()))
        .thenReturn(PROVIDER_TEST);
    when(noConfController.contextsEqual(anyInt(), anyObject(), anyInt(), anyObject()))
        .thenReturn(true);
    when(noConfController.isConfigurable()).thenReturn(false);
    when(mControllers.get(eq(~TRACE_CONTROLLER_ID))).thenReturn(noConfController);

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
  }

  @Test
  public void testStartFromInsideTraceFails() {
    TraceController secondController = mock(TraceController.class);
    when(secondController.getNonConfigurableProviders(anyLong(), anyObject()))
        .thenReturn(PROVIDER_TEST);
    when(secondController.contextsEqual(anyInt(), anyObject(), anyInt(), anyObject()))
        .thenReturn(true);
    when(secondController.isConfigurable()).thenReturn(true);
    when(mControllers.get(eq(~TRACE_CONTROLLER_ID))).thenReturn(secondController);

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
    assertTracing();

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertThat(mTraceControl.startTrace(~TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertTracing();
  }

  @Test
  public void testStartChecksControllerNonconfigurable() {
    when(mNonconfigurableController.getNonConfigurableProviders(anyLong(), anyObject()))
        .thenReturn(0);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertNotTracing();

    when(mNonconfigurableController.getNonConfigurableProviders(anyLong(), anyObject()))
        .thenReturn(PROVIDER_TEST);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
    assertTracing();
  }

  @Test
  public void testStartChecksControllerConfigurable() {
    Object context = new Object();
    setController(mConfigurableController);

    when(mConfigurableController.findTraceConfigIdx(anyLong(), same(context), same(mConfig)))
        .thenReturn(TraceController.RESULT_COINFLIP_MISS);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isFalse();
    assertNotTracing();

    when(mConfigurableController.findTraceConfigIdx(anyLong(), same(context), same(mConfig)))
        .thenReturn(TraceController.RESULT_NO_TRACE_CONFIG);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isFalse();
    assertNotTracing();

    when(mConfigurableController.findTraceConfigIdx(anyLong(), same(context), same(mConfig)))
        .thenReturn(0);

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    assertTracing();

    TraceContext traceContext =
        mTraceControl.getTraceContextByTrigger(TRACE_CONTROLLER_ID, 0, context);
    assertThat(traceContext).isNotNull();
    assertThat(traceContext.traceConfigIdx).isEqualTo(0);
    assertThat(traceContext.config).isSameAs(mConfig);
    assertThat(traceContext.mTraceConfigExtras.getIntParam(TEST_INT_PARAM_KEY, 0))
        .isEqualTo(TEST_INT_PARAM_VALUE);
  }

  @Test
  public void testStopNeedsSameContext() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    assertTracing();

    // Different context doesn't stop the trace..
    when(mNonconfigurableController.contextsEqual(anyLong(), anyObject(), anyLong(), anyObject()))
        .thenReturn(false);
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, new Object(), 0);
    assertTracing();

    // ..but the right context does
    when(mNonconfigurableController.contextsEqual(anyLong(), anyObject(), anyLong(), anyObject()))
        .thenReturn(true);
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, context, 0);
    assertNotTracing();
  }

  @Test
  public void testStopDifferentController() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    assertTracing();

    // Different controller id doesn't stop the trace
    mTraceControl.stopTrace(~TRACE_CONTROLLER_ID, context, 0);
    assertTracing();
  }

  @Test
  public void testStopControllerMask() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    assertTracing();

    int mask = (TRACE_CONTROLLER_ID << 2) | TRACE_CONTROLLER_ID;
    mTraceControl.stopTrace(mask, context, 0);
    assertNotTracing();
  }

  @Test
  public void testStartWritesControlEvent() {
    int flags = 0xFACEB00C & ~Trace.FLAG_MEMORY_ONLY; // MEMORY_ONLY trace is special
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, flags, new Object(), 0)).isTrue();

    verifyStatic(BufferLogger.class);
    long traceId = anyLong();
    BufferLogger.writeAndWakeupTraceWriter(
        any(NativeTraceWriter.class),
        same(mBuffer),
        traceId,
        EntryType.TRACE_START,
        anyInt(),
        eq(flags),
        traceId);

    verify(mTraceControlHandler).onTraceStart(any(TraceContext.class), anyInt());
  }

  @Test
  public void testStartBlackBoxRecordingTrace() {
    int flags = Trace.FLAG_MEMORY_ONLY;
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, flags, new Object(), 0)).isTrue();

    verifyStatic(BufferLogger.class, times(1));
    long traceId = anyLong();
    BufferLogger.writeAndWakeupTraceWriter(
        any(NativeTraceWriter.class),
        same(mBuffer),
        traceId,
        EntryType.TRACE_START,
        anyInt(),
        eq(flags),
        traceId);
    verifyStatic(BufferLogger.class, never());
    long traceId1 = anyLong();
    BufferLogger.writeAndWakeupTraceWriter(
        any(NativeTraceWriter.class),
        same(mBuffer),
        traceId1,
        EntryType.TRACE_BACKWARDS,
        0,
        eq(flags),
        traceId1);

    verify(mTraceControlHandler).onTraceStart(any(TraceContext.class), eq(Integer.MAX_VALUE));
    assertMemoryOnlyTracing();
  }

  @Test
  public void testMultipleTracesStartStop() {
    long flag1 = 1;
    long flag2 = 2;
    when(mNonconfigurableController.contextsEqual(eq(flag1), isNull(), eq(flag2), isNull()))
        .thenReturn(false);
    when(mNonconfigurableController.contextsEqual(eq(flag2), isNull(), eq(flag1), isNull()))
        .thenReturn(false);

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, null, flag1)).isTrue();
    assertNormalTracing();
    assertMemoryOnlyNotTracing();
    TraceContext traceContext1 = mTraceControl.getCurrentTraces().get(0);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, Trace.FLAG_MEMORY_ONLY, null, flag2))
        .isTrue();
    assertMemoryOnlyTracing();
    TraceContext traceContext2 = mTraceControl.getCurrentTraces().get(1);
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, null, flag1);

    assertNormalNotTracing();
    assertMemoryOnlyTracing();
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, null, flag2);
    ArgumentCaptor<TraceContext> contextCaptor = ArgumentCaptor.forClass(TraceContext.class);
    verify(mTraceControlHandler, times(2)).onTraceStop(contextCaptor.capture());
    assertThat(contextCaptor.getAllValues().get(0).longContext).isEqualTo(flag1);
    assertThat(contextCaptor.getAllValues().get(1).longContext).isEqualTo(flag2);
    assertNotTracing();
  }

  @Test
  public void testStopWritesControlEvent() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, context, 0);

    verify(mTraceControlHandler).onTraceStop(any(TraceContext.class));
  }

  @Test
  public void testAbortWritesControlEvent() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    mTraceControl.abortTrace(TRACE_CONTROLLER_ID, context, 0);

    verifyStatic(BufferLogger.class);
    BufferLogger.writeStandardEntry(
        same(mBuffer),
        anyInt(),
        EntryType.TRACE_ABORT,
        anyInt(),
        anyInt(),
        anyInt(),
        anyInt(),
        anyLong());

    verify(mTraceControlHandler).onTraceAbort(any(TraceContext.class));
  }

  @Test
  public void testConfigChangeOutsideTraceWritesNothing() {
    TestConfigProvider provider = new TestConfigProvider();
    mTraceControl.setConfig(provider.getFullConfig());

    verifyStatic(BufferLogger.class, never());
    BufferLogger.writeStandardEntry(
        any(Buffer.class), anyInt(), anyInt(), anyInt(), anyInt(), anyInt(), anyInt(), anyLong());
  }

  @Test
  public void testConfigChangeInsideTraceContinuesTrace() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();

    TestConfigProvider provider = new TestConfigProvider();
    mTraceControl.setConfig(provider.getFullConfig());

    assertTracing();
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, context, 0);
    assertNotTracing();
  }

  @Test
  public void testCleanupByID() {
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
    long id = mTraceControl.getCurrentTraceIDs()[0];
    mTraceControl.cleanupTraceContextByID(id, ProfiloConstants.ABORT_REASON_CONTROLLER_INITIATED);

    assertNotTracing();

    verify(mTraceControlHandler).onTraceAbort(any(TraceContext.class));
    verifyStatic(BufferLogger.class, never());
    BufferLogger.writeStandardEntry(
        any(Buffer.class),
        anyInt(),
        EntryType.TRACE_ABORT,
        anyInt(),
        anyInt(),
        anyInt(),
        anyInt(),
        anyLong());
  }

  @Test
  public void testTwoConcurrentNormalTracesAreNotAllowed() {
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, null, 0)).isTrue();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, null, 1)).isFalse();
  }

  @Test
  public void testTwoConcurrentMemoryOnlyTracesAreNotAllowed() {
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, Trace.FLAG_MEMORY_ONLY, null, 0))
        .isTrue();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, Trace.FLAG_MEMORY_ONLY, null, 1))
        .isFalse();
  }

  @Test
  public void testNormalAndMemoryOnlyConcurrentTracesAreAllowed() {
    long flag1 = 1;
    long flag2 = 2;
    when(mNonconfigurableController.contextsEqual(eq(flag1), any(), eq(flag2), any()))
        .thenReturn(false);
    when(mNonconfigurableController.contextsEqual(eq(flag2), any(), eq(flag1), any()))
        .thenReturn(false);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, Trace.FLAG_MEMORY_ONLY, null, flag1))
        .isTrue();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, null, flag2)).isTrue();
  }

  private void assertNormalNotTracing() {
    assertThat(mTraceControl.isInsideNormalTrace()).isFalse();
  }

  private void assertMemoryOnlyNotTracing() {
    assertThat(mTraceControl.isInsideMemoryOnlyTrace()).isFalse();
  }

  private void assertNotTracing() {
    assertThat(mTraceControl.isInsideTrace()).isFalse();
    assertThat(mTraceControl.getCurrentTraces().isEmpty());
  }

  private void assertNormalTracing() {
    assertThat(mTraceControl.isInsideNormalTrace()).isTrue();
  }

  private void assertMemoryOnlyTracing() {
    assertThat(mTraceControl.isInsideMemoryOnlyTrace()).isTrue();
  }

  private void assertTracing() {
    assertThat(mTraceControl.isInsideTrace()).isTrue();
    assertThat(!mTraceControl.getCurrentTraces().isEmpty());
  }
}
