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

#include <cppdistract/dso.h>

namespace facebook {

using namespace cppdistract;

dso const& libart() {
  static dso const libart("libart.so");
  return libart;
}

dso const& libcxx() {
  static dso const libcxx("libc++.so");
  return libcxx;
}

dso const& libc() {
  static dso const libc("libc.so");
  return libc;
}

dso const& libnativehelper() {
  static dso const libnativehelper("libnativehelper.so");
  return libnativehelper;
}

dso const& libm() {
  static dso const libm("libm.so");
  return libm;
}

dso const& libhwui() {
  static dso const libhwui("libhwui.so");
  return libhwui;
}
dso const& libutils() {
  static dso const libutils("libutils.so");
  return libutils;
}

dso const& libandroid() {
  static dso const libandroid("libandroid.so");
  return libandroid;
}

dso const& libcutils() {
  static dso const libcutils("libcutils.so");
  return libcutils;
}

dso const& libandroid_runtime() {
  static dso const libandroid_runtime("libandroid_runtime.so");
  return libandroid_runtime;
}

} // namespace facebook
