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

#include <string>

#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <mappingdensity/mappingdensity.h>

namespace {

void dumpMappingDensities(
    facebook::jni::alias_ref<jclass>,
    std::string mapRegex,
    std::string outFile) {
  facebook::profilo::mappingdensity::dumpMappingDensities(
      mapRegex, outFile);
}

} // namespace

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void*) {
  constexpr char const* kMappingDensityProvider =
      "com/facebook/profilo/provider/mappingdensity/MappingDensityProvider";
  return facebook::xplat::initialize(vm, [] {
    facebook::jni::registerNatives(
        kMappingDensityProvider,
        {
            makeNativeMethod("dumpMappingDensities", dumpMappingDensities),
        });
  });
}
