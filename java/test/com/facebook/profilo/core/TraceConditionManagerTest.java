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
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import com.facebook.profilo.config.ConfigImpl;
import com.facebook.profilo.config.ConfigParams;
import com.facebook.profilo.ipc.TraceConfigExtras;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.mmapbuf.Buffer;
import com.facebook.testing.powermock.PowerMockTest;
import com.facebook.testing.robolectric.v4.WithTestDefaultsRunner;
import java.io.File;
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
            new ConfigImpl(0, new ConfigParams()),
            0,
            null,
            null,
            0,
            0,
            0,
            0,
            mExtras,
            /*buffer*/ null,
            new Buffer[] {},
            new File("."),
            "prefix-");
  }

  @Test
  public void testDefaultTraceConditionManager() {
    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testTraceWithNoConfig() {
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(null);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testTraceWithConfigDidNotProcessDuration() {
    int[] config = new int[] {3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    // This trace actually has a duration condition configured, but it's not met
    // (i.e, we never called processDurationEvent()), so it returns 0
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
  }

  @Test
  public void testTraceWithConfigDurationLessThanConfigured() {
    int[] config = new int[] {3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    tcm.processDurationEvent(DUMMY_TRACE_ID, 2999);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
  }

  @Test
  public void testTraceWithConfigDurationGreaterButWrongMarker() {
    int[] config = new int[] {3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    // Equivalent to never calling processDurationEvent on this marker
    tcm.processDurationEvent(DUMMY_TRACE_ID + 1, 3999);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
  }

  @Test
  public void testTraceWithConfigDurationGreater() {
    int[] config = new int[] {3000, 2};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    tcm.processDurationEvent(DUMMY_TRACE_ID, 3001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(2);
  }

  @Test
  public void testTraceWithManyConfigs() {
    int[] config = new int[] {3000, 1, 2500, 2, 2000, 4, 1500, 8, 1000, 16};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();

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
    int[] config = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();

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
    int[] config = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    tcm.processDurationEvent(DUMMY_TRACE_ID, 500);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);
    tcm.processDurationEvent(DUMMY_TRACE_ID, 1001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(16);
    // Durations should be monotonically increasing, so this is not something
    // that should happen. But, still, make sure we return the right value
    tcm.processDurationEvent(DUMMY_TRACE_ID, 500);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(16);
  }

  @Test
  public void testAnnotationConditionAny() {
    String[] config = new String[] {"annotation", "any", "KEY:VAL1", "KEY:VAL2"};
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    // First, let's try with the wrong key
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "WRONG_KEY:VAL1");
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);

    // Correct key, correct value
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "KEY:VAL1");
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);

    // Since this is ANY, using the other value should work too
    tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "KEY:VAL2");
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testAnnotationConditionAll() {
    String[] config = new String[] {"annotation", "all", "KEY:VAL1", "KEY:VAL2"};
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(config);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();

    // First, let's try with the wrong key
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "WRONG_KEY:VAL1");
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);

    // Correct key, correct value. However, the operation is "ALL", so this
    // will still return 0
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "KEY:VAL1");
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);

    // Now we've seen all the annotations
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "KEY:VAL2");
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testFallbackSamplingRateForAnnotations() {
    String[] config = new String[] {"annotation", "all", "KEY:VAL1", "KEY:VAL2"};
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(config);
    when(mExtras.getIntParam(
            eq(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION_FALLBACK_SAMPLING_RATE),
            anyInt()))
        .thenReturn(20);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();

    // Since the annotation condition is not met, we'll return the fallback value
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(20);
  }

  @Test
  public void testFallbackSamplingRateWithDurationCondition() {
    // This should behave exactly as testFallbackSamplingRateForAnnotations,
    // even though this scenario also has a duration condition and neither
    // the duration nor the annotation conditions were met
    String[] config = new String[] {"annotation", "all", "KEY:VAL1", "KEY:VAL2"};
    int[] durationConfig = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(durationConfig);
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(config);
    when(mExtras.getIntParam(
            eq(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION_FALLBACK_SAMPLING_RATE),
            anyInt()))
        .thenReturn(20);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();

    // Since the annotation condition is not met, we'll return the fallback value
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(20);
  }

  @Test
  public void testMixedAnnotationsAndDurations() {
    String[] annotationsConfig = new String[] {"annotation", "any", "KEY:VAL1", "KEY:VAL2"};
    int[] durationConfig = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(durationConfig);
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(annotationsConfig);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isTrue();

    // If the duration is met but annotation conditions aren't, then we won't
    // upload the trace
    tcm.processDurationEvent(DUMMY_TRACE_ID, 1501);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(0);

    // Let's now pass an annotation
    tcm.processAnnotationEvent(DUMMY_TRACE_ID, "KEY:VAL2");

    // Now we should return the sampling rate established in the
    // duration condition
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(8);

    // If we had seen a longer duration, we'd return that
    tcm.processDurationEvent(DUMMY_TRACE_ID, 3001);
    assertThat(tcm.getUploadSampleRate(DUMMY_TRACE_ID)).isEqualTo(1);
  }

  @Test
  public void testMalformedDurationCondition() {
    // Odd number of parameters. Trace registration should fail.
    int[] durationConfig = new int[] {1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000 /*, 1*/};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(durationConfig);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isFalse();
  }

  @Test
  public void testMalformedAnnotationConditionNotEnoughParameters() {
    // Fewer than 3 parameters. Trace registration should fail
    String[] annotationsConfig = new String[] {"annotation", "any" /*, "KEY:VAL1", "KEY:VAL2"*/};
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(annotationsConfig);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isFalse();
  }

  @Test
  public void testMalformedAnnotationConditionNegativeFallbackSampleRate() {
    // Fewer than 3 parameters. Trace registration should fail
    String[] annotationsConfig = new String[] {"annotation", "any", "KEY:VAL1", "KEY:VAL2"};
    when(mExtras.getStringArrayParam(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION))
        .thenReturn(annotationsConfig);
    when(mExtras.getIntParam(
            eq(ProfiloConstants.TRACE_CONFIG_STRING_LIST_CONDITION_FALLBACK_SAMPLING_RATE),
            anyInt()))
        .thenReturn(-1);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isFalse();
  }

  @Test
  public void testMalformedDurationConditionNegativeDuration() {
    int[] durationConfig = new int[] {-1000, 16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(durationConfig);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isFalse();
  }

  @Test
  public void testMalformedDurationConditionNegativeSampleRate() {
    int[] durationConfig = new int[] {1000, -16, 1500, 8, 2000, 4, 2500, 2, 3000, 1};
    when(mExtras.getIntArrayParam(ProfiloConstants.TRACE_CONFIG_DURATION_CONDITION))
        .thenReturn(durationConfig);

    TraceConditionManager tcm = new TraceConditionManager();
    assertThat(tcm.registerTrace(mTraceContext)).isFalse();
  }
}
