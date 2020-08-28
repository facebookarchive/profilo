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

  // System config parameters
  int getSystemConfigParamInt(String key);

  int optSystemConfigParamInt(String key, int defaultVal);

  boolean getSystemConfigParamBool(String key);

  boolean optSystemConfigParamBool(String key, boolean defaultVal);

  String getSystemConfigParamString(String key);

  @Nullable
  String optSystemConfigParamString(String key, @Nullable String defaultVal);

  int[] getSystemConfigParamIntList(String key);

  @Nullable
  int[] optSystemConfigParamIntList(String key);

  String[] getSystemConfigParamStringList(String key);

  String[] optSystemConfigParamStringList(String key);

  // Trace config parameters
  int[] getTraceConfigIdxs(String triggerType, String triggerAction);

  String[] getTraceConfigProviders(int traceConfigIdx);

  int getTraceConfigParamInt(int traceConfigIdx, String key);

  int optTraceConfigParamInt(int traceConfigIdx, String key, int defaultVal);

  boolean getTraceConfigParamBool(int traceConfigIdx, String key);

  boolean optTraceConfigParamBool(int traceConfigIdx, String key, boolean defaultVal);

  String getTraceConfigParamString(int traceConfigIdx, String key);

  @Nullable
  String optTraceConfigParamString(int traceConfigIdx, String key, @Nullable String defaultVal);

  int[] getTraceConfigParamIntList(int traceConfigIdx, String key);

  @Nullable
  int[] optTraceConfigParamIntList(int traceConfigIdx, String key);

  String[] getTraceConfigParamStringList(int traceConfigIdx, String key);

  @Nullable
  String[] optTraceConfigParamStringList(int traceConfigIdx, String key);

  // Trigger parameters
  int getTraceConfigTriggerParamInt(
      int traceConfigIdx, String triggerType, String triggerAction, String key);

  int optTraceConfigTriggerParamInt(
      int traceConfigIdx, String triggerType, String triggerAction, String key, int defaultVal);

  boolean getTraceConfigTriggerParamBool(
      int traceConfigIdx, String triggerType, String triggerAction, String key);

  boolean optTraceConfigTriggerParamBool(
      int traceConfigIdx, String triggerType, String triggerAction, String key, boolean defaultVal);

  String getTraceConfigTriggerParamString(
      int traceConfigIdx, String triggerType, String triggerAction, String key);

  @Nullable
  String optTraceConfigTriggerParamString(
      int traceConfigIdx,
      String triggerType,
      String triggerAction,
      String key,
      @Nullable String defaultVal);

  int[] getTraceConfigTriggerParamIntList(
      int traceconfigIdx, String triggerType, String triggerAction, String key);

  @Nullable
  int[] optTraceConfigTriggerParamIntList(
      int traceconfigIdx, String triggerType, String triggerAction, String key);

  String[] getTraceConfigTriggerParamStringList(
      int traceconfigIdx, String triggerType, String triggerAction, String key);

  @Nullable
  String[] optTraceConfigTriggerParamStringList(
      int traceconfigIdx, String triggerType, String triggerAction, String key);

  /** Only use when serializing a TraceConfig. The explicit accessors are much faster. */
  ConfigParams getTraceConfigParams(int traceConfigIdx);

  // Misc
  boolean isDisablingConfig();

  int getVersion();

  long getID();
}
