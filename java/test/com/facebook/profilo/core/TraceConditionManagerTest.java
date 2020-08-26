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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import com.facebook.profilo.ipc.TraceConfigExtras;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.util.TestConfigProvider;
import com.facebook.testing.powermock.PowerMockTest;
import com.facebook.testing.robolectric.v4.WithTestDefaultsRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.powermock.core.classloader.annotations.PrepareForTest;

@PrepareForTest({TraceConfigExtras.class})
@RunWith(WithTestDefaultsRunner.class)
public class TraceConditionManagerTest extends PowerMockTest {
  private final int DUMMY_TRACE_ID = 1234;

  private TraceConfigExtras mExtras;
  private TraceContext mTraceContext;

  @Before
  public void setUp() {
    mExtras = mock(TraceConfigExtras.class);
    mTraceContext =
        new TraceContext(
            DUMMY_TRACE_ID,
            "",
            new TestConfigProvider().getFullConfig(),
            0,
            null,
            null,
            0,
            0,
            0,
            0,
            mExtras);
  }

  @Test
  public void testDefaultTraceConditionManager() {
    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testTraceWithNoConfig() {
    TraceConditionManager tcm = new TraceConditionManager();
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(null);

    tcm.registerTrace(mTraceContext);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testTraceWithConfigDidNotProcessDuration() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);
    // This trace actually has a duration condition configured, but it's not met
    // (i.e, we never called processDurationEvent()), so it returns 0
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
  }

  @Test
  public void testTraceWithConfigDurationLessThanConfigured() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);
    tcm.processDurationEvent(DUMMY_TRACE_ID, 2999);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
  }

  @Test
  public void testTraceWithConfigDurationGreaterButWrongMarker() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);
    // Equivalent to never calling processDurationEvent on this marker
    tcm.processDurationEvent(DUMMY_TRACE_ID + 1, 3999);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
  }

  @Test
  public void testTraceWithConfigDurationGreater() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {3000, 2};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);
    tcm.processDurationEvent(DUMMY_TRACE_ID, 3001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(2);
  }

  @Test
  public void testTraceWithManyConfigs() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {3000, 1, 2500, 2, 2000, 4, 1500, 8, 1000, 16};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 500);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 1001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(16);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 1501);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(8);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 2001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(4);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 2501);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(2);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 3001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testTraceWithManyConfigsNotInOrder() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 500);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 1001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(16);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 1501);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(8);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 2001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(4);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 2501);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(2);

    tcm.processDurationEvent(DUMMY_TRACE_ID, 3001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testManyDurationEvents() {
    TraceConditionManager tcm = new TraceConditionManager();
    int[] config = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    tcm.registerTrace(mTraceContext);
    tcm.processDurationEvent(DUMMY_TRACE_ID, 500);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
    tcm.processDurationEvent(DUMMY_TRACE_ID, 1001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(16);
    // Durations should be monotonically increasing, so this is not something
    // that should happen. But, still, make sure we return the right value
    tcm.processDurationEvent(DUMMY_TRACE_ID, 500);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(16);
  }
}
