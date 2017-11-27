// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.jni;

import com.facebook.common.errorreporting.ErrorReportingGkReader;
import com.facebook.common.errorreporting.FbErrorReporter;
import com.facebook.common.errorreporting.SoftError;
import com.facebook.common.util.TriState;
import com.facebook.proguard.annotations.DoNotStrip;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

/**
 * Static class that acts as a proxy for native SoftError reporting. This simply passes on the soft
 * error to the registered error reporter
 */
@DoNotStrip
public final class NativeSoftErrorReporterProxy {
  private static final String NATIVE_SOFT_ERROR_TAG = "[Native] ";
  private static final int MAX_SOFT_ERROR_CACHE_SIZE = 50;

  private static WeakReference<FbErrorReporter> sFbErrorReporter;
  private static LinkedList<SoftError> sSoftErrorCache = new LinkedList<>();

  /* Severity levels and tags */
  private static final int SEVERITY_WARNING = 1;
  private static final int SEVERITY_MUSTFIX = 2;
  private static final int SEVERITY_ASSERT = 3;
  private static final String SEVERITY_WARNING_TAG = "<level:warning> ";
  private static final String SEVERITY_MUSTFIX_TAG = "<level:mustfix> ";
  private static final String SEVERITY_ASSERT_TAG = "<level:assert> ";
  private static final String SEVERITY_UNKNOWN_TAG = "<level:unknown> ";

  private static ErrorReportingGkReader sErrorReportingGkReader;

  private NativeSoftErrorReporterProxy() {}

  public static synchronized void registerErrorReporter(
      FbErrorReporter fbErrorReporter, ErrorReportingGkReader errorReportingGkReader) {
    sErrorReportingGkReader = errorReportingGkReader;

    if (sFbErrorReporter == null) {
      sFbErrorReporter = new WeakReference<>(fbErrorReporter);
      flushSoftErrorCache();
    }
  }

  @DoNotStrip
  public static void softReport(
      final int severity,
      final String category,
      final String message,
      final int samplingFrequency) {
    softReport(severity, category, message, null, samplingFrequency);
  }

  @DoNotStrip
  public static void softReport(
      final int severity,
      final String category,
      final String message,
      final Throwable cause,
      final int samplingFrequency) {

    FbErrorReporter fbErrorReporter = null;
    if (sFbErrorReporter != null) {
      fbErrorReporter = sFbErrorReporter.get();
    }

    if (fbErrorReporter == null || shouldReportNativeSoftErrors() == TriState.UNSET) {
      insertSoftErrorIntoCache(
          getNativeCategoryString(severity, category), message, cause, samplingFrequency);
    } else if (shouldReportNativeSoftErrors() == TriState.YES) {
      fbErrorReporter.softReport(
          getNativeCategoryString(severity, category), message, cause, samplingFrequency);
    }
  }

  private static String getSeverityTag(final int severity) {
    switch (severity) {
      case SEVERITY_WARNING:
        return SEVERITY_WARNING_TAG;
      case SEVERITY_MUSTFIX:
        return SEVERITY_MUSTFIX_TAG;
      case SEVERITY_ASSERT:
        return SEVERITY_ASSERT_TAG;
      default:
        return SEVERITY_UNKNOWN_TAG;
    }
  }

  private static String getNativeCategoryString(final int severity, final String category) {
    return NATIVE_SOFT_ERROR_TAG + getSeverityTag(severity) + category;
  }

  private static TriState shouldReportNativeSoftErrors() {
    return (sErrorReportingGkReader == null)
        ? TriState.UNSET
        : sErrorReportingGkReader.shouldReportNativeSoftErrors();
  }

  private static synchronized void insertSoftErrorIntoCache(
      final String category,
      final String message,
      final Throwable cause,
      final int samplingFrequency) {
    // Restrict the size of the list to the max and keep it as a rolling list
    if (sSoftErrorCache.size() == MAX_SOFT_ERROR_CACHE_SIZE) {
      sSoftErrorCache.removeFirst();
    }

    sSoftErrorCache.addLast(
        SoftError.newBuilder(category, message)
            .setCause(cause)
            .setSamplingFrequency(samplingFrequency)
            .build());
  }

  private static synchronized void flushSoftErrorCache() {
    if (sFbErrorReporter != null) {
      FbErrorReporter fbErrorReporter = sFbErrorReporter.get();

      if (fbErrorReporter != null) {
        TriState reportNativeSoftErrors = shouldReportNativeSoftErrors();
        if (reportNativeSoftErrors == TriState.YES) {
          for (SoftError softError : sSoftErrorCache) {
            fbErrorReporter.softReport(softError);
          }
        }

        if (reportNativeSoftErrors != TriState.UNSET) {
          sSoftErrorCache.clear();
        }
      }
    }
  }

  static synchronized List<SoftError> getCachedSoftErrors() {
    return new ArrayList<>(sSoftErrorCache);
  }

  /** This generates soft errors from native code and should be used only for testing purposes. */
  @DoNotStrip
  public static native void generateNativeSoftError();
}
