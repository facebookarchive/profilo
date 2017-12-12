// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace facebook {
namespace loom {
namespace writer {

enum AbortReason {
  UNKNOWN = 1,
  CONTROLLER_INITIATED = 2,
  MISSED_EVENT = 3,
  TIMEOUT = 4,
  NEW_START = 5,
  REMOTE_PROCESS = 6,
};

} // namespace writer
} // namespace loom
} // namespace facebook
