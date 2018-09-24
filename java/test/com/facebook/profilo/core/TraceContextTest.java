/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.facebook.profilo.core;

import static org.assertj.core.api.Java6Assertions.assertThat;

import android.os.Parcel;
import com.facebook.profilo.ipc.TraceContext;
import com.facebook.testing.robolectric.v3.WithTestDefaultsRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(WithTestDefaultsRunner.class)
public class TraceContextTest {

  private static final long TRACE_ID = 12345L;
  private static final String ENCODED_TRACE_ID = "trace_id";
  private static final int CONTROLLER = 11111;
  private static final Object CONTROLLER_OBJECT = "controller_object";
  private static final Object CONTEXT = "context";
  private static final int INT_CONTEXT = 22222;
  private static final int ENABLED_PROVIDERS = 33333;
  private static final int CPU_SAMPLING_RATE_MS = 44444;
  private static final int FLAGS = 1;
  private static final int ABORT_REASON = 1;

  private TraceContext mContext;

  @Before
  public void setUp() {
    mContext = new TraceContext(
        TRACE_ID,
        ENCODED_TRACE_ID,
        CONTROLLER,
        CONTROLLER_OBJECT,
        CONTEXT,
        INT_CONTEXT,
        ENABLED_PROVIDERS,
        CPU_SAMPLING_RATE_MS,
        FLAGS,
        ABORT_REASON);
  }

  @Test
  public void testGetterMethods() {
    assertThat(mContext.traceId).isEqualTo(TRACE_ID);
    assertThat(mContext.encodedTraceId).isEqualTo(ENCODED_TRACE_ID);
    assertThat(mContext.controller).isEqualTo(CONTROLLER);
    assertThat(mContext.controllerObject).isEqualTo(CONTROLLER_OBJECT);
    assertThat(mContext.context).isEqualTo(CONTEXT);
    assertThat(mContext.longContext).isEqualTo(INT_CONTEXT);
    assertThat(mContext.enabledProviders).isEqualTo(ENABLED_PROVIDERS);
    assertThat(mContext.cpuSamplingRateMs).isEqualTo(CPU_SAMPLING_RATE_MS);
    assertThat(mContext.flags).isEqualTo(FLAGS);
    assertThat(mContext.abortReason).isEqualTo(ABORT_REASON);
  }

  @Test
  public void testParcel() {
    Parcel parcel = Parcel.obtain();
    mContext.writeToParcel(parcel, 0);
    // After you're done with writing, you need to reset the parcel for reading:
    parcel.setDataPosition(0);
    TraceContext createdFromParcel = TraceContext.CREATOR.createFromParcel(parcel);
    assertThat(createdFromParcel.traceId).isEqualTo(TRACE_ID);
    assertThat(createdFromParcel.encodedTraceId).isEqualTo(ENCODED_TRACE_ID);
    assertThat(createdFromParcel.controller).isEqualTo(CONTROLLER);
    assertThat(createdFromParcel.controllerObject).isNull();
    assertThat(createdFromParcel.context).isNull();
    assertThat(createdFromParcel.longContext).isEqualTo(INT_CONTEXT);
    assertThat(createdFromParcel.enabledProviders).isEqualTo(ENABLED_PROVIDERS);
    assertThat(createdFromParcel.cpuSamplingRateMs).isEqualTo(CPU_SAMPLING_RATE_MS);
    assertThat(createdFromParcel.abortReason).isEqualTo(ABORT_REASON);
  }
}
