/*
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

#pragma once

#include <fb/Build.h>
#include <fb/log.h>
#include <sigmux.h>
#include <linux_syscall_support.h>

namespace facebook {
namespace profilo {
namespace sigmuxsetup {

namespace {
static int sys_sigaction(int signum, const struct sigaction* act,
    struct sigaction* oldact) {
#ifdef __LP64__
  return syscall(
      __NR_rt_sigaction,
      signum,
      act,
      oldact,
      sizeof(sigset_t));
#else
  return syscall(
      __NR_sigaction,
      signum,
      act,
      oldact);
#endif 
}
} // namespace anonymous

inline void EnsureSigmuxOverridesArtFaultHandler() {

  static constexpr auto kL = 21;
  static constexpr auto kNMR1 = 25;
  auto sdk = build::Build::getAndroidSdk();
  if (sdk >= kL && sdk <= kNMR1) {
    // art's FaultHandler is broken until 8.0 in the following way:
    //
    // When it encounters a SIGSEGV, it blindly reads r0 from the faulting
    // ucontext and assumes that it's an art::ArtMethod pointer (it does so
    // in the IsInGeneratedCode function).
    //
    // If the value in r0 happens to align to a 16-byte boundary and it happens
    // to be on an art thread, art will then proceed to dereference r0 to
    // find its declaring class.
    //
    // Of course, in our case, *anything* can be in r0, so art can crash in
    // situations that we can recover from.
    //
    // This all happens before our (or sigmux's) signal handler can run.
    //
    // Fixed upstream in 143f61c29e77328e19bcdba3cc94df7334c40358, first
    // included in 8.0.0_r1.
    //
    // We can work around this bug by telling sigmux to use our supplied
    // sigaction(3) which directly calls into the kernel, instead of the
    // framework's (which is libsigchain's).
    //
    // Therefore, we'll have sigmux run before the art handler.
    //
    // This has process-wide implications and may be addressed otherwise in the
    // future (T30664695).
    //
    FBLOGD("Applying FaultHandler workaround");
    auto old_fn = sigmux_set_real_sigaction(&sys_sigaction);
    if (old_fn != nullptr) {
      // Assume someone else beat us to the punch and revert our change.
      FBLOGD("Reverting FaultHandler workaround, assuming original was safe");
      sigmux_set_real_sigaction(old_fn);
    }
  }
}

}
}
}
