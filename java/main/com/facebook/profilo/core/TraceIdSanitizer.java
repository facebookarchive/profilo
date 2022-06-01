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

/**
 * Provides operations to convert between base64-encoded Trace IDs and file/folder names.
 *
 * <p>This class is not meant to be instantiated.
 *
 * <p>The transformation is designed to be reversible for diagnostic purposes.
 */
public final class TraceIdSanitizer {
  private static final String SANITIZED_PLUS = "_p_";
  private static final String SANITIZED_SLASH = "_s_";

  /**
   * Converts the Trace ID into a string that can safely be used as part of a directory name.
   *
   * <p>Base64 uses alphanumeric characters as well as '+' and '/'. In filenames, we replace '+'
   * with '_p_' and '/' with '_s_'.
   *
   * @param traceId The base64-encoded Trace ID to convert.
   * @return A string consisting only of alphanumeric characters and '_'.
   */
  public static String sanitizeTraceId(String traceId) {
    return traceId.replace("+", SANITIZED_PLUS).replace("/", SANITIZED_SLASH);
  }

  /**
   * Returns the original Trace ID corresponding to the given sanitized Trace ID.
   *
   * @seealso sanitizeTraceId
   */
  public static String restoreSanitizedTraceId(String sanitizedTraceId) {
    return sanitizedTraceId.replace(SANITIZED_PLUS, "+").replace(SANITIZED_SLASH, "/");
  }
}
