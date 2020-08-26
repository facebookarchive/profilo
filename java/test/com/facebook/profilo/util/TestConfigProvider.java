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
package com.facebook.profilo.util;

import com.facebook.profilo.config.Config;
import com.facebook.profilo.config.ConfigImpl;
import com.facebook.profilo.config.ConfigParams;
import com.facebook.profilo.config.ConfigProvider;

public final class TestConfigProvider implements ConfigProvider {

  private Config mConfig;

  public TestConfigProvider() {
    this(new ConfigImpl(0, new ConfigParams()));
  }

  public TestConfigProvider(Config config) {
    mConfig = config;
  }

  public void setConfig(Config config) {
    mConfig = config;
  }

  @Override
  public Config getFullConfig() {
    return mConfig;
  }
}
