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
package com.facebook.profilo.config;

import javax.annotation.Nullable;

public interface Config {

  @Nullable
  ConfigV2 getConfigV2();

  interface RootControllerConfig {

    @Nullable
    ControllerConfig getConfigForController(int controller);

    /**
     * @return a maximum duration (timeout) for traces or -1 if none is set. The amount is in
     *     milliseconds.
     */
    int getTraceTimeoutMs();

    /**
     * @return Sample rate at which to upload timed out traces. Value N means 1/N probability of
     *     timed out trace to be uploaded. Default value is 0, which means that such traces should
     *     be aborted.
     */
    int getTimedOutUploadSampleRate();
  }
}
