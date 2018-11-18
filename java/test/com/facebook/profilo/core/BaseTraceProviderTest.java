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

import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.eq;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.when;

import com.facebook.profilo.ipc.TraceContext;
import com.facebook.testing.robolectric.v3.WithTestDefaultsRunner;
import java.io.File;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.internal.util.reflection.Whitebox;
import org.powermock.core.classloader.annotations.PrepareForTest;

@PrepareForTest({
    TraceEvents.class,
})
@RunWith(WithTestDefaultsRunner.class)
public class BaseTraceProviderTest {

  public static final TraceContext TRACE_CONTEXT = new TraceContext();
  public static final File EXTRA_FILE = new File("/");
  public static final int PROVIDER_ID = 1;

  @Mock private BaseTraceProvider mTestProvider;

  @Before
  public void setUp() {
    MockitoAnnotations.initMocks(this);

    mockStatic(TraceEvents.class);
    when(TraceEvents.enabledMask(anyInt())).thenReturn(0xFFFFFFFF);

    when(mTestProvider.getSupportedProviders()).thenReturn(PROVIDER_ID);
    Whitebox.setInternalState(mTestProvider, "mSolibInitialized", true);
  }

  @Test
  public void testTraceProviderLifecycle() {
    mTestProvider.onEnable(TRACE_CONTEXT, EXTRA_FILE);
    mTestProvider.onDisable(TRACE_CONTEXT, EXTRA_FILE);

    InOrder enablingOrder = Mockito.inOrder(mTestProvider);
    enablingOrder.verify(mTestProvider).enable();
    enablingOrder.verify(mTestProvider).onTraceStarted(eq(TRACE_CONTEXT), eq(EXTRA_FILE));

    InOrder disablingOrder = Mockito.inOrder(mTestProvider);
    disablingOrder.verify(mTestProvider).onTraceEnded(eq(TRACE_CONTEXT), eq(EXTRA_FILE));
    disablingOrder.verify(mTestProvider).disable();
  }
}
