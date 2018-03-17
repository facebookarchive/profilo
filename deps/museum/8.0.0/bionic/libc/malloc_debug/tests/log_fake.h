/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef MALLOC_DEBUG_TESTS_LOG_FAKE_H
#define MALLOC_DEBUG_TESTS_LOG_FAKE_H

#include <string>

void resetLogs();
std::string getFakeLogBuf();
std::string getFakeLogPrint();

#endif // MALLOC_DEBUG_TESTS_LOG_FAKE_H
