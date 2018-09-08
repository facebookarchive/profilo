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

#pragma once

/**
 * The number of method entries in the stack trace per a single stack trace to
 * report
 */
#define MAX_STACK_DEPTH 255

/**
 * The maximum number of frames to track stack traces
 */
#define MAX_STACKS_COUNT 24

/**
 * Number of full stacks upon which should flush stacks to the Profilo buffer
 */
#define FLUSH_STACKS_COUNT 8
