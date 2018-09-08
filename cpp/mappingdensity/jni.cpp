// Copyright 2004-present Facebook. All Rights Reserved.

#include <string>

#include <fb/xplat_init.h>
#include <fbjni/fbjni.h>
#include <mappingdensity/mappingdensity.h>

namespace {

void dumpMappingDensities(
    facebook::jni::alias_ref<jclass>,
    std::string mapRegex,
    std::string outFile,
    std::string dumpName) {
  facebook::profilo::mappingdensity::dumpMappingDensities(
      mapRegex, outFile, dumpName);
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
