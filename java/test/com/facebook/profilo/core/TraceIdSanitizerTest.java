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

import static com.facebook.profilo.core.TraceIdSanitizer.restoreSanitizedTraceId;
import static com.facebook.profilo.core.TraceIdSanitizer.sanitizeTraceId;
import static org.assertj.core.api.Assertions.assertThat;

import com.facebook.testing.robolectric.v4.WithTestDefaultsRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Tests {@link TraceIdSanitizer} */
@RunWith(WithTestDefaultsRunner.class)
public class TraceIdSanitizerTest {
  @Before
  public void setUp() {}

  @Test
  public void testSanitizeTraceId() {
    assertThat(sanitizeTraceId("")).isEqualTo("");
    assertThat(sanitizeTraceId("+")).isEqualTo("_p_");
    assertThat(sanitizeTraceId("/")).isEqualTo("_s_");
    assertThat(sanitizeTraceId("aAmMzZ059")).isEqualTo("aAmMzZ059");
    assertThat(sanitizeTraceId("a+b/c")).isEqualTo("a_p_b_s_c");
  }

  @Test
  public void testRestoreSanitizedTraceId() {
    assertThat(restoreSanitizedTraceId("")).isEqualTo("");
    assertThat(restoreSanitizedTraceId("_p_")).isEqualTo("+");
    assertThat(restoreSanitizedTraceId("_s_")).isEqualTo("/");
    assertThat(restoreSanitizedTraceId("aAmMzZ059")).isEqualTo("aAmMzZ059");
    assertThat(restoreSanitizedTraceId("a_p_b_s_c")).isEqualTo("a+b/c");
  }
}
