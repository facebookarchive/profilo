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

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.powermock.api.mockito.PowerMockito.doNothing;
import static org.powermock.api.mockito.PowerMockito.doReturn;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.spy;
import static org.powermock.api.mockito.PowerMockito.when;

import com.facebook.profilo.ipc.TraceContext;
import com.facebook.testing.robolectric.v3.WithTestDefaultsRunner;
import java.io.File;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.internal.util.reflection.Whitebox;
import org.powermock.core.classloader.annotations.PrepareForTest;

@PrepareForTest({
  TraceEvents.class,
  BaseTraceProvider.class,
})
@RunWith(WithTestDefaultsRunner.class)
public class BaseTraceProviderTest {

  public static final TraceContext TRACE_CONTEXT = new TraceContext();
  public static final File EXTRA_FILE = new File("/");
  public static final int PROVIDER_ID = 1;

  private BaseTraceProvider mTestProvider;
  private BaseTraceProvider.ExtraDataFileProvider mExtraFileProvider;

  @Before
  public void setUp() {
    mockStatic(TraceEvents.class);
    when(TraceEvents.enabledMask(anyInt())).thenReturn(0xFFFFFFFF);

    mExtraFileProvider = mock(BaseTraceProvider.ExtraDataFileProvider.class);
    mTestProvider = spy(new TestTraceProvider());

    doReturn(PROVIDER_ID).when(mTestProvider).getSupportedProviders();

    // Need to stub the methods below as a workaround for proper method counting.
    doNothing().when(mTestProvider).enable();
    doNothing().when(mTestProvider).disable();
    doNothing()
        .when(mTestProvider)
        .onTraceStarted(
            any(TraceContext.class), any(BaseTraceProvider.ExtraDataFileProvider.class));
    doNothing()
        .when(mTestProvider)
        .onTraceEnded(any(TraceContext.class), any(BaseTraceProvider.ExtraDataFileProvider.class));

    Whitebox.setInternalState(mTestProvider, "mSolibInitialized", true);

    when(mExtraFileProvider.getExtraDataFile(any(TraceContext.class), any(BaseTraceProvider.class)))
        .thenReturn(EXTRA_FILE);
  }

  @Test
  public void testTraceProviderLifecycle() {
    mTestProvider.onEnable(TRACE_CONTEXT, mExtraFileProvider);
    mTestProvider.onDisable(TRACE_CONTEXT, mExtraFileProvider);

    InOrder enablingOrder = Mockito.inOrder(mTestProvider);
    enablingOrder.verify(mTestProvider).enable();
    enablingOrder.verify(mTestProvider).onTraceStarted(eq(TRACE_CONTEXT), eq(mExtraFileProvider));

    InOrder disablingOrder = Mockito.inOrder(mTestProvider);
    disablingOrder.verify(mTestProvider).onTraceEnded(eq(TRACE_CONTEXT), eq(mExtraFileProvider));
    disablingOrder.verify(mTestProvider).disable();
  }

  @Test
  public void testProviderEarlyExitOnEnableIfProviderMaskIsNotTracing() throws Exception {
    when(TraceEvents.enabledMask(anyInt())).thenReturn(0x0);
    mTestProvider.onEnable(TRACE_CONTEXT, mExtraFileProvider);
    verify(mTestProvider, never()).ensureSolibLoaded();
    verify(mTestProvider, never()).enable();
    verify(mTestProvider, never()).onTraceStarted(eq(TRACE_CONTEXT), eq(mExtraFileProvider));
  }

  @Test
  public void testProviderEarlyExitOnDisableIfNoActiveTracingMask() {
    Whitebox.setInternalState(mTestProvider, "mSavedProviders", 0);
    mTestProvider.onDisable(TRACE_CONTEXT, mExtraFileProvider);
    verify(mTestProvider, never()).ensureSolibLoaded();
    verify(mTestProvider, never()).disable();
    verify(mTestProvider, never()).onTraceEnded(eq(TRACE_CONTEXT), eq(mExtraFileProvider));
  }

  static class TestTraceProvider extends BaseTraceProvider {
    @Override
    protected int getTracingProviders() {
      return 0;
    }

    @Override
    protected int getSupportedProviders() {
      return PROVIDER_ID;
    }

    @Override
    protected void enable() {}

    @Override
    protected void disable() {}
  }
}
