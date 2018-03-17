/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_TI_AGENT_H_
#define ART_RUNTIME_TI_AGENT_H_

#include <dlfcn.h>
#include <jni.h>  // for jint, JavaVM* etc declarations

#include "runtime.h"
#include "utils.h"

namespace art {
namespace ti {

using AgentOnLoadFunction = jint (*)(JavaVM*, const char*, void*);
using AgentOnUnloadFunction = void (*)(JavaVM*);

// Agents are native libraries that will be loaded by the runtime for the purpose of
// instrumentation. They will be entered by Agent_OnLoad or Agent_OnAttach depending on whether the
// agent is being attached during runtime startup or later.
//
// The agent's Agent_OnUnload function will be called during runtime shutdown.
//
// TODO: consider splitting ti::Agent into command line, agent and shared library handler classes
// TODO Support native-bridge. Currently agents can only be the actual runtime ISA of the device.
class Agent {
 public:
  enum LoadError {
    kNoError,              // No error occurred..
    kAlreadyStarted,       // The agent has already been loaded.
    kLoadingError,         // dlopen or dlsym returned an error.
    kInitializationError,  // The entrypoint did not return 0. This might require an abort.
  };

  bool IsStarted() const {
    return dlopen_handle_ != nullptr;
  }

  const std::string& GetName() const {
    return name_;
  }

  const std::string& GetArgs() const {
    return args_;
  }

  bool HasArgs() const {
    return !GetArgs().empty();
  }

  void* FindSymbol(const std::string& name) const;

  LoadError Load(/*out*/jint* call_res, /*out*/std::string* error_msg) {
    VLOG(agents) << "Loading agent: " << name_ << " " << args_;
    return DoLoadHelper(false, call_res, error_msg);
  }

  // TODO We need to acquire some locks probably.
  void Unload();

  // Tries to attach the agent using its OnAttach method. Returns true on success.
  LoadError Attach(/*out*/jint* call_res, /*out*/std::string* error_msg) {
    VLOG(agents) << "Attaching agent: " << name_ << " " << args_;
    return DoLoadHelper(true, call_res, error_msg);
  }

  explicit Agent(std::string arg);

  Agent(const Agent& other);
  Agent& operator=(const Agent& other);

  Agent(Agent&& other);
  Agent& operator=(Agent&& other);

  ~Agent();

 private:
  LoadError DoDlOpen(/*out*/std::string* error_msg);

  LoadError DoLoadHelper(bool attaching,
                         /*out*/jint* call_res,
                         /*out*/std::string* error_msg);

  std::string name_;
  std::string args_;
  void* dlopen_handle_;

  // The entrypoints.
  AgentOnLoadFunction onload_;
  AgentOnLoadFunction onattach_;
  AgentOnUnloadFunction onunload_;

  friend std::ostream& operator<<(std::ostream &os, Agent const& m);
};

std::ostream& operator<<(std::ostream &os, Agent const& m);
std::ostream& operator<<(std::ostream &os, const Agent* m);

}  // namespace ti
}  // namespace art

#endif  // ART_RUNTIME_TI_AGENT_H_

