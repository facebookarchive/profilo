#include <museum/7.0.0/art/runtime/base/mutex.h>
#include <new>

namespace facebook { namespace museum { namespace MUSEUM_VERSION { namespace art {

// these methods are specifically no-op'ed because profilo only _ever_ reads.
// it mustn't ever mutate ART state, which includes changing lock states.

void Mutex::ExclusiveLock(Thread* self) {
  // no op
}

void Mutex::ExclusiveUnlock(Thread* self) {
  // no op
}

void ConditionVariable::Wait(Thread* self) {
  // no op
}

} } } } // namespace facebook::museum::MUSEUM_VERSION::art
