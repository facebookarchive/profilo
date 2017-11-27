// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import static org.assertj.core.api.Java6Assertions.assertThat;

import com.facebook.loom.controllers.ColdStartController;
import com.facebook.quicklog.QuickEvent;
import org.junit.Before;
import org.junit.Test;

public class ColdStartTraceControllerTest {
  private static final int MARKER_ID = 0xFACEB00C;

  private QuickEvent mQuickEvent;
  private ColdStartController mTraceController;

  @Before
  public void setup() {
    mQuickEvent = QuickEvent.allocateInactiveEvent(
        MARKER_ID,
        1,
        System.nanoTime(),
        false,
        false,
        1,
        false);
    mTraceController = new ColdStartController();
  }

  @Test
  public void testContextEquality() {
    assertThat(mTraceController.contextsEqual(MARKER_ID, null, 0, mQuickEvent)).isTrue();
    assertThat(mTraceController.contextsEqual(0, null, 0, null)).isTrue();
  }

  @Test
  public void testContextInequality() {
    assertThat(mTraceController.contextsEqual(MARKER_ID, new Object(), 0, mQuickEvent)).isFalse();
    assertThat(mTraceController.contextsEqual(MARKER_ID, new Object(), 0, null)).isFalse();
    assertThat(mTraceController.contextsEqual(1, new Object(), 0, null)).isFalse();
    assertThat(mTraceController.contextsEqual(1, null, 0, null)).isFalse();
    assertThat(mTraceController.contextsEqual(1, null, 2, null)).isFalse();
    assertThat(mTraceController.contextsEqual(1, new Object(), 2, null)).isFalse();
    assertThat(mTraceController.contextsEqual(1, new Object(), 2, new Object())).isFalse();
  }

}
