// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.core;

import javax.annotation.Nullable;

/**
 * Marker interface for controllers that can check if they're inside a trace
 * related to a QPL marker
 */
public interface ControllerWithQPLChecks {

  boolean isInsideQPLTrace(int intContext, @Nullable Object context, int marker);
}
