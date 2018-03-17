/*
 * Copyright (C) 2009 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_UTILS_H_
#define ART_RUNTIME_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_UTILS_H_

namespace unix_file {

class RandomAccessFile;

// Copies from 'src' to 'dst'. Reads all the data from 'src', and writes it
// to 'dst'. Not thread-safe. Neither file will be closed.
bool CopyFile(const RandomAccessFile& src, RandomAccessFile* dst);

}  // namespace unix_file

#endif  // ART_RUNTIME_BASE_UNIX_FILE_RANDOM_ACCESS_FILE_UTILS_H_
