// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import static org.assertj.core.api.Java6Assertions.assertThat;
import static org.powermock.api.mockito.PowerMockito.mock;
import static org.powermock.api.mockito.PowerMockito.when;

import com.facebook.loom.config.json.QPLControllerConfig;
import com.facebook.loom.config.json.QPLTraceControlConfiguration;
import com.facebook.loom.controllers.sequencelogger.SequenceLoggerTraceController;
import com.facebook.sequencelogger.SequenceDefinition;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.powermock.modules.junit4.PowerMockRunner;

@RunWith(PowerMockRunner.class)
public class SequenceTraceControllerTest {

  @Mock SequenceDefinition mSequenceDefinition;
  @Mock QPLControllerConfig mController;
  @Mock QPLTraceControlConfiguration mConfig;

  private static final int MOCK_MARKER_ID = 0xf0001;

  @Before
  public void setUp() {
    when(mSequenceDefinition.getSequenceIdentifier()).thenReturn(MOCK_MARKER_ID);
  }

  private void setConfigForMarker(boolean enableConfig, int sampleRate) {
    if (!enableConfig) {
      when(mController.getConfigForMarker(MOCK_MARKER_ID)).thenReturn(null);
      return;
    }
    when(mController.getConfigForMarker(MOCK_MARKER_ID)).thenReturn(mConfig);
    when(mConfig.getCoinflipSampleRate()).thenReturn(sampleRate);
    when(mConfig.getEnabledEventProviders()).thenReturn(2);
    when(mConfig.getCpuSamplingRateMs()).thenReturn(10);
  }

  private SequenceDefinition createSequenceDefinition(int markerId) {
    SequenceDefinition sequenceDefinition = mock(SequenceDefinition.class);
    when(sequenceDefinition.getSequenceIdentifier()).thenReturn(markerId);
    return sequenceDefinition;
  }

  @Test
  public void testEvaluateNoConfig() {
    setConfigForMarker(false, 0);

    int provider = new SequenceLoggerTraceController().
        evaluateConfig(mSequenceDefinition, mController);
    assertThat(provider).isEqualTo(0);
  }

  @Test
  public void testEvaluateNotSampled() {
    setConfigForMarker(true, 0);

    int provider = new SequenceLoggerTraceController().
        evaluateConfig(mSequenceDefinition, mController);
    assertThat(provider).isEqualTo(0);
  }

  @Test
  public void testEvaluateSampled() {
    setConfigForMarker(true, 1);

    int provider = new SequenceLoggerTraceController().
        evaluateConfig(mSequenceDefinition, mController);
    assertThat(provider).isEqualTo(2);
  }

  @Test
  public void testEvaluateEquals() {
   SequenceLoggerTraceController controller = new SequenceLoggerTraceController();
   SequenceDefinition sequenceDefinition = createSequenceDefinition(MOCK_MARKER_ID);

   assertThat(controller.contextsEqual(0, mSequenceDefinition, 0, mSequenceDefinition)).isTrue();
   assertThat(controller.contextsEqual(0, mSequenceDefinition, 0, sequenceDefinition)).isTrue();
   assertThat(controller.contextsEqual(0, null, 0, null)).isTrue();
  }

  @Test
  public void testEvaluateNotEquals() {
   SequenceLoggerTraceController controller = new SequenceLoggerTraceController();
   SequenceDefinition sequenceDefinition = createSequenceDefinition(MOCK_MARKER_ID+1);

   assertThat(controller.contextsEqual(10, mSequenceDefinition, 0, mSequenceDefinition)).isFalse();
   assertThat(controller.contextsEqual(0, mSequenceDefinition, 0, sequenceDefinition)).isFalse();
   assertThat(controller.contextsEqual(0, mSequenceDefinition, 0, sequenceDefinition)).isFalse();
   assertThat(controller.contextsEqual(0, mSequenceDefinition, 0, new Object())).isFalse();
   assertThat(controller.contextsEqual(0, new Object(), 0, sequenceDefinition)).isFalse();
   assertThat(controller.contextsEqual(0, mSequenceDefinition, 0, null)).isFalse();
   assertThat(controller.contextsEqual(0, null, 0, mSequenceDefinition)).isFalse();
  }

  @Test
  public void testCpuSamplingRateConfig() {
    setConfigForMarker(true, 1);

    int cpuSamplingRate = new SequenceLoggerTraceController()
        .getCpuSamplingRateMs(mSequenceDefinition, mController);
    assertThat(cpuSamplingRate).isEqualTo(10);
  }
}
