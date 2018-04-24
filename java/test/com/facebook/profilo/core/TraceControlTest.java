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
import static org.junit.Assert.fail;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyLong;
import static org.mockito.Matchers.anyObject;
import static org.mockito.Matchers.eq;
import static org.mockito.Matchers.same;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.verifyStatic;
import static org.powermock.api.mockito.PowerMockito.when;

import android.util.SparseArray;
import com.facebook.fbtrace.utils.FbTraceId;
import com.facebook.profilo.config.Config;
import com.facebook.profilo.config.ControllerConfig;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.logger.Logger;
import com.facebook.profilo.logger.Trace;
import com.facebook.profilo.util.TestConfigProvider;
import com.facebook.testing.robolectric.v3.WithTestDefaultsRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.internal.util.reflection.Whitebox;
import org.powermock.core.classloader.annotations.PrepareForTest;
import org.powermock.core.classloader.annotations.SuppressStaticInitializationFor;

@RunWith(WithTestDefaultsRunner.class)
@PrepareForTest({
  Logger.class,
  TraceControl.class,
})
@SuppressStaticInitializationFor("com.facebook.profilo.logger.Logger")
public class TraceControlTest {

  private static final int TRACE_CONTROLLER_ID = 100;
  private static final long TEST_TRACE_ID = 10000l;

  private static final int PROVIDER_TEST = ProvidersRegistry.newProvider("test");

  private TraceControl mTraceControl;
  private SparseArray<TraceController> mControllers;
  private TraceController mController;
  private TraceControlHandler mTraceControlHandler;
  private TraceContext mTraceContext;
  private final TestConfigProvider mTraceConfigProvider =
      new TestConfigProvider().setControllers(TRACE_CONTROLLER_ID);
  private final ControllerConfig mControllerConfig =
      mTraceConfigProvider
          .getFullConfig()
          .getControllersConfig()
          .getConfigForController(TRACE_CONTROLLER_ID);
  private final Config mConfig = mTraceConfigProvider.getFullConfig();

  @Before
  public void setUp() throws Exception {
    mockStatic(Logger.class);

    mController = mock(TraceController.class);

    when(mController.evaluateConfig(anyObject(), same(mControllerConfig)))
        .thenReturn(PROVIDER_TEST);
    when(mController.contextsEqual(anyInt(), anyObject(), anyInt(), anyObject()))
        .thenReturn(true);
    //noinspection unchecked
    mControllers = mock(SparseArray.class);
    when(mControllers.get(eq(TRACE_CONTROLLER_ID))).thenReturn(mController);
    when(mControllers.get(eq(TRACE_CONTROLLER_ID), any(TraceController.class)))
        .thenReturn(mController);

    mTraceControl =
        new TraceControl(mControllers, mConfig, mock(TraceControl.TraceControlListener.class));

    mTraceControlHandler = mock(TraceControlHandler.class);
    Whitebox.setInternalState(mTraceControl, "mTraceControlHandler", mTraceControlHandler);

    mTraceContext =
        new TraceContext(
            TEST_TRACE_ID,
            FbTraceId.encode(TEST_TRACE_ID),
            1111,
            new Object(),
            new Object(),
            1,
            PROVIDER_TEST,
            100,
            1,
            222);
  }

  @Test
  public void testStartWithExistingTraceContext() {
    assertThat(mTraceControl.adoptContext(TRACE_CONTROLLER_ID, 0, mTraceContext)).isTrue();
    assertTracing();

    TraceContext currContext = mTraceControl.getCurrentTraces().get(0);
    assertThat(currContext).isNotNull();
    assertThat(currContext.traceId).isEqualTo(mTraceContext.traceId);
    assertThat(currContext.encodedTraceId).isEqualTo(mTraceContext.encodedTraceId);
    assertThat(currContext.controller).isEqualTo(TRACE_CONTROLLER_ID);
    assertThat(currContext.controllerObject).isEqualTo(mController);
    assertThat(currContext.context).isEqualTo(mTraceContext.context);
    assertThat(currContext.intContext).isEqualTo(mTraceContext.intContext);
    assertThat(currContext.enabledProviders).isEqualTo(mTraceContext.enabledProviders);
    assertThat(currContext.cpuSamplingRateMs).isEqualTo(mTraceContext.cpuSamplingRateMs);
  }

  @Test
  public void testStartFiltersOutControllers() {
    assertThat(mTraceControl.startTrace(~TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertNotTracing();

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
    assertTracing();
  }

  @Test(expected = RuntimeException.class)
  public void testThrowsOnConfigWithoutController() {
    when(mControllers.get(eq(TRACE_CONTROLLER_ID))).thenReturn(null);
    mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0);
    fail("TraceControl did not throw when querying for unregistered controllerID");
  }

  @Test
  public void testStartFromInsideTraceFails() {
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isTrue();
    assertTracing();

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertThat(mTraceControl.startTrace(~TRACE_CONTROLLER_ID, 0, new Object(), 0)).isFalse();
    assertTracing();
  }

  @Test
  public void testStartChecksController() {
    when(mController.evaluateConfig(anyObject(), any(ControllerConfig.class)))
        .thenReturn(0);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0))
        .isFalse();
    assertNotTracing();

    when(mController.evaluateConfig(anyObject(), any(ControllerConfig.class)))
        .thenReturn(PROVIDER_TEST);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, new Object(), 0))
        .isTrue();
    assertTracing();
  }

  @Test
  public void testStopNeedsSameContext() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();
    assertTracing();

    // Different context doesn't stop the trace..
    when(mController.contextsEqual(anyInt(), anyObject(), anyInt(), anyObject())).thenReturn(false);
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, new Object(), 0);
    assertTracing();

    // ..but the right context does
    when(mController.contextsEqual(anyInt(), anyObject(), anyInt(), anyObject())).thenReturn(true);
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

    verifyStatic();
    Logger.postCreateTrace(anyLong(), eq(flags), anyInt());

    verify(mTraceControlHandler)
        .onTraceStart(
            any(TraceContext.class), eq(mConfig.getControllersConfig().getTraceTimeoutMs()));
  }

  @Test
  public void testStartBlackBoxRecordingTrace() {
    int flags = Trace.FLAG_MEMORY_ONLY;
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, flags, new Object(), 0)).isTrue();

    verifyStatic(never());
    Logger.postCreateTrace(anyLong(), eq(flags), anyInt());
    verifyStatic(never());
    Logger.postCreateBackwardTrace(anyLong());

    verify(mTraceControlHandler).onTraceStart(any(TraceContext.class), eq(Integer.MAX_VALUE));
  }

  @Test
  public void testMultipleTracesStartStop() {
    int flag1 = 1;
    int flag2 = 2;
    when(mController.contextsEqual(eq(flag1), any(), eq(flag2), any())).thenReturn(false);
    when(mController.contextsEqual(eq(flag2), any(), eq(flag1), any())).thenReturn(false);

    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, null, flag1)).isTrue();
    TraceContext traceContext1 = mTraceControl.getCurrentTraces().get(0);
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, null, flag2)).isTrue();
    TraceContext traceContext2 = mTraceControl.getCurrentTraces().get(1);
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, null, flag1);

    assertTracing();
    mTraceControl.stopTrace(TRACE_CONTROLLER_ID, null, flag2);
    ArgumentCaptor<TraceContext> contextCaptor = ArgumentCaptor.forClass(TraceContext.class);
    verify(mTraceControlHandler, times(2)).onTraceStop(contextCaptor.capture());
    assertThat(contextCaptor.getAllValues().get(0).intContext).isEqualTo(flag1);
    assertThat(contextCaptor.getAllValues().get(1).intContext).isEqualTo(flag2);
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

    verifyStatic();
    Logger.postAbortTrace(anyLong());

    verify(mTraceControlHandler).onTraceAbort(any(TraceContext.class));
  }

  @Test
  public void testConfigChangeOutsideTraceWritesNothing() {
    TestConfigProvider provider = new TestConfigProvider().setControllers(1, 2);
    mTraceControl.setConfig(provider.getFullConfig());

    verifyStatic(never());
    Logger.postAbortTrace(anyLong());
  }

  @Test
  public void testConfigChangeInsideTraceContinuesTrace() {
    Object context = new Object();
    assertThat(mTraceControl.startTrace(TRACE_CONTROLLER_ID, 0, context, 0)).isTrue();

    TestConfigProvider provider = new TestConfigProvider().setControllers(1, 2);
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
    verifyStatic(never());
    Logger.postAbortTrace(anyLong());
  }

  private void assertNotTracing() {
    assertThat(mTraceControl.isInsideTrace()).isFalse();
    assertThat(mTraceControl.getCurrentTraces().isEmpty());
  }

  private void assertTracing() {
    assertThat(mTraceControl.isInsideTrace()).isTrue();
    assertThat(!mTraceControl.getCurrentTraces().isEmpty());
  }
}
