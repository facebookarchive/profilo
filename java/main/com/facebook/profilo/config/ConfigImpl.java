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

import java.util.Arrays;
import java.util.TreeMap;
import javax.annotation.Nullable;

public class ConfigImpl implements Config {

  private final long mId;
  private final ConfigParams mSystemConfig;
  private final ConfigTraceConfig[] mTraceConfigs;

  public ConfigImpl(long id, ConfigParams systemConfig, ConfigTraceConfig... traceConfigs) {
    mId = id;
    mSystemConfig = systemConfig;
    mTraceConfigs = traceConfigs;
  }

  private static <T> T getParam(@Nullable TreeMap<String, T> map, String key) {
    if (map != null && map.containsKey(key)) {
      return map.get(key);
    }
    throw new IllegalArgumentException("Parameter " + key + " not found");
  }

  @Nullable
  private static <T> T optParam(
      @Nullable TreeMap<String, T> map, String key, @Nullable T defaultValue) {
    if (map != null && map.containsKey(key)) {
      return map.get(key);
    }
    return defaultValue;
  }

  private ConfigParams getTriggerParams(int traceConfigIdx, String type, String action) {
    @Nullable ConfigTraceConfig.Trigger[] triggers = mTraceConfigs[traceConfigIdx].triggers;
    if (triggers == null) {
      return ConfigParams.EMPTY;
    }
    for (ConfigTraceConfig.Trigger trigger : triggers) {
      if (trigger.type.equals(type) && trigger.action.equals(action)) {
        return trigger.params;
      }
    }
    return ConfigParams.EMPTY;
  }

  @Override
  public int getSystemConfigParamInt(String key) {
    return getParam(mSystemConfig.intParams, key);
  }

  @Override
  public int optSystemConfigParamInt(String key, int defaultVal) {
    return optParam(mSystemConfig.intParams, key, defaultVal);
  }

  @Override
  public boolean getSystemConfigParamBool(String key) {
    return getParam(mSystemConfig.boolParams, key);
  }

  @Override
  public boolean optSystemConfigParamBool(String key, boolean defaultVal) {
    return optParam(mSystemConfig.boolParams, key, defaultVal);
  }

  @Override
  public String getSystemConfigParamString(String key) {
    return getParam(mSystemConfig.stringParams, key);
  }

  @Override
  @Nullable
  public String optSystemConfigParamString(String key, @Nullable String defaultVal) {
    return optParam(mSystemConfig.stringParams, key, defaultVal);
  }

  @Override
  public int[] getSystemConfigParamIntList(String key) {
    return getParam(mSystemConfig.intListParams, key);
  }

  @Override
  @Nullable
  public int[] optSystemConfigParamIntList(String key) {
    return optParam(mSystemConfig.intListParams, key, null);
  }

  @Override
  public String[] getSystemConfigParamStringList(String key) {
    return getParam(mSystemConfig.stringListParams, key);
  }

  @Override
  public String[] optSystemConfigParamStringList(String key) {
    return optParam(mSystemConfig.stringListParams, key, null);
  }

  @Override
  public int[] getTraceConfigIdxs(String triggerType, String triggerAction) {
    int[] result = new int[mTraceConfigs.length];
    int successes = 0;

    for (int idx = 0; idx < mTraceConfigs.length; idx++) {
      boolean pass = false;
      @Nullable ConfigTraceConfig.Trigger[] triggers = mTraceConfigs[idx].triggers;
      if (triggers == null) {
        continue;
      }
      for (ConfigTraceConfig.Trigger trigger : triggers) {
        if (trigger.type.equals(triggerType) && trigger.action.equals(triggerAction)) {
          pass = true;
        }
      }
      if (pass) {
        result[successes++] = idx;
      }
    }
    return Arrays.copyOf(result, successes);
  }

  @Override
  public String[] getTraceConfigProviders(int traceConfigIdx) {
    @Nullable String[] result = mTraceConfigs[traceConfigIdx].enabledProviders;
    if (result == null) {
      return new String[0];
    }
    return result;
  }

  @Override
  public int getTraceConfigParamInt(int traceConfigIdx, String key) {
    return getParam(mTraceConfigs[traceConfigIdx].params.intParams, key);
  }

  @Override
  public int optTraceConfigParamInt(int traceConfigIdx, String key, int defaultVal) {
    return optParam(mTraceConfigs[traceConfigIdx].params.intParams, key, defaultVal);
  }

  @Override
  public boolean getTraceConfigParamBool(int traceConfigIdx, String key) {
    return getParam(mTraceConfigs[traceConfigIdx].params.boolParams, key);
  }

  @Override
  public boolean optTraceConfigParamBool(int traceConfigIdx, String key, boolean defaultVal) {
    return optParam(mTraceConfigs[traceConfigIdx].params.boolParams, key, defaultVal);
  }

  @Override
  public String getTraceConfigParamString(int traceConfigIdx, String key) {
    return getParam(mTraceConfigs[traceConfigIdx].params.stringParams, key);
  }

  @Override
  @Nullable
  public String optTraceConfigParamString(
      int traceConfigIdx, String key, @Nullable String defaultVal) {
    return optParam(mTraceConfigs[traceConfigIdx].params.stringParams, key, defaultVal);
  }

  @Override
  public int[] getTraceConfigParamIntList(int traceConfigIdx, String key) {
    return getParam(mTraceConfigs[traceConfigIdx].params.intListParams, key);
  }

  @Override
  public int[] optTraceConfigParamIntList(int traceConfigIdx, String key) {
    return optParam(mTraceConfigs[traceConfigIdx].params.intListParams, key, null);
  }

  @Override
  public String[] getTraceConfigParamStringList(int traceConfigIdx, String key) {
    return getParam(mTraceConfigs[traceConfigIdx].params.stringListParams, key);
  }

  @Override
  public String[] optTraceConfigParamStringList(int traceConfigIdx, String key) {
    return optParam(mTraceConfigs[traceConfigIdx].params.stringListParams, key, null);
  }

  @Override
  public int getTraceConfigTriggerParamInt(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return getParam(getTriggerParams(traceConfigIdx, triggerType, triggerAction).intParams, key);
  }

  @Override
  public int optTraceConfigTriggerParamInt(
      int traceConfigIdx, String triggerType, String triggerAction, String key, int defaultVal) {
    return optParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).intParams, key, defaultVal);
  }

  @Override
  public boolean getTraceConfigTriggerParamBool(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return getParam(getTriggerParams(traceConfigIdx, triggerType, triggerAction).boolParams, key);
  }

  @Override
  public boolean optTraceConfigTriggerParamBool(
      int traceConfigIdx,
      String triggerType,
      String triggerAction,
      String key,
      boolean defaultVal) {
    return optParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).boolParams, key, defaultVal);
  }

  @Override
  public String getTraceConfigTriggerParamString(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return getParam(getTriggerParams(traceConfigIdx, triggerType, triggerAction).stringParams, key);
  }

  @Override
  @Nullable
  public String optTraceConfigTriggerParamString(
      int traceConfigIdx,
      String triggerType,
      String triggerAction,
      String key,
      @Nullable String defaultVal) {
    return optParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).stringParams, key, defaultVal);
  }

  @Override
  public int[] getTraceConfigTriggerParamIntList(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return getParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).intListParams, key);
  }

  @Override
  public int[] optTraceConfigTriggerParamIntList(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return optParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).intListParams, key, null);
  }

  @Override
  public String[] getTraceConfigTriggerParamStringList(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return getParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).stringListParams, key);
  }

  @Override
  public String[] optTraceConfigTriggerParamStringList(
      int traceConfigIdx, String triggerType, String triggerAction, String key) {
    return optParam(
        getTriggerParams(traceConfigIdx, triggerType, triggerAction).stringListParams, key, null);
  }

  @Override
  public ConfigParams getTraceConfigParams(int traceConfigIdx) {
    return getTraceConfigParams(traceConfigIdx);
  }

  @Override
  public boolean isDisablingConfig() {
    return false;
  }

  @Override
  public int getVersion() {
    return 3;
  }

  @Override
  public long getID() {
    return mId;
  }
}
