#include <jni.h>

namespace facebook {
namespace profilo {
namespace detail {

// These flags should match the ones from Logger.java
static constexpr uint32_t FILL_TIMESTAMP = 1 << 1;
static constexpr uint32_t FILL_TID = 1 << 2;

jint loggerWriteStandardEntry(
    JNIEnv* env,
    jobject cls,
    jint flags,
    jint type,
    jlong timestamp,
    jint tid,
    jint arg1,
    jint arg2,
    jlong arg3);

jint loggerWriteBytesEntry(
    JNIEnv* env,
    jobject cls,
    jint flags,
    jint type,
    jint arg1,
    jstring arg2);

}}}
