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

import java.util.concurrent.TimeUnit;

public class DefaultConfigProvider implements ConfigProvider {

  public static final int DEFAULT_TIMED_OUT_UPLOAD_SAMPLE_RATE = 0;
  public static final int DEFAULT_TRACE_TIMEOUT_MS = 30000;
  public static final int DEFAULT_MAX_BYTES = 10000;
  public static final long DEFAULT_UPLOAD_PERIOD_SEC = TimeUnit.HOURS.toSeconds(1);
  public static final int DEFAULT_UPLOAD_BYTES_PER_UPDATE = 10000 / 24;
  public static final int DEFAULT_BUFFER_SIZE = -1;
  public static final boolean DEFAULT_IS_MMAP_BUFFER = false;

  public static final Config DEFAULT_CONFIG = new ConfigImpl(0, new ConfigParams());

  @Override
  public Config getFullConfig() {
    return DEFAULT_CONFIG;
  }
}
