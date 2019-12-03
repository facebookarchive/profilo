#include "MmapBufferManager.h"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  return facebook::jni::initialize(vm, [] {
    facebook::profilo::mmapbuf::MmapBufferManager::registerNatives();
  });
}
