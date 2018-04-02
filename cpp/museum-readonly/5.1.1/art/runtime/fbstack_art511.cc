#include <museum/5.1.1/external/libcxx/cstring>

#include <museum/5.1.1/art/runtime/fbstack_art511.h>

#include <museum/5.1.1/art/runtime/runtime.h>
#include <museum/5.1.1/art/runtime/stack.h>
#include <museum/5.1.1/art/runtime/thread.h>
#include <museum/5.1.1/art/runtime/mirror/art_method-inl.h>
#include <museum/5.1.1/art/runtime/entrypoints/quick/quick_entrypoints.h>

#include <museum/5.1.1/art/runtime/fbentrypoints.h>

#include <new>

using namespace facebook::museum::MUSEUM_VERSION::art;

using namespace entrypoints;
void* HostEntryPoints::quick_entrypoints = nullptr;

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {
namespace entrypoints {

static JavaVMExt* JavaVmExtFromEnv(JNIEnv* env) {
  return reinterpret_cast<JNIEnvExt*>(env)->vm;
}

static Runtime* GetRuntimeFromEnv(JNIEnv* env) {
  return JavaVmExtFromEnv(env)->runtime;
}

void InstallRuntime(JNIEnv* env, void* thread) {
  Thread* artThread = reinterpret_cast<Thread*>(thread);

  QuickEntryPoints* host = new QuickEntryPoints(artThread->GetQuickEntryPoints());
  HostEntryPoints::quick_entrypoints = host;
}

size_t GetStackTrace(JavaFrame* frames, size_t max_frames, void* thread) {
  struct InplaceStackVisitor: public StackVisitor {
    InplaceStackVisitor(JavaFrame* frames, size_t max_frames, Thread* thread):
      StackVisitor(thread, nullptr),
      frames_(frames),
      max_frames_(max_frames),
      idx_(0)
    {}

    bool VisitFrame() {
      if (idx_ == max_frames_) {
        // Overflow
        return false;
      }
      auto frame = GetMethod();
      if (frame->IsRuntimeMethod()) {
        // Skip methods we can't symbolicate.
        return true;
      }

      JavaFrame entry {};
      entry.methodIdx = (uint32_t) frame->GetDexMethodIndex();
      // Read top 4 bytes of the sha1 signature in little-endian.
      entry.dexSignature =
        *((uint32_t*) frame->GetDexCache()->GetDexFile()->GetHeader().signature_);

      frames_[idx_++] = entry;
      return true;
    }

    JavaFrame* frames_;
    size_t max_frames_;
    size_t idx_;
  };

  InplaceStackVisitor visitor(frames, max_frames, reinterpret_cast<Thread*>(thread));
  visitor.WalkStack();
  return visitor.idx_;
}

} // namespace entrypoints
} } } } // namespace facebook::museum::MUSEUM_VERSION::art
