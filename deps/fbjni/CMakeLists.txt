# Copyright (c) Facebook, Inc. and its affiliates.
#
# This source code is licensed under the MIT license found in the LICENSE file
# in the root directory of this source tree.

cmake_minimum_required(VERSION 3.6.0)
project(fbjni CXX)

file(GLOB fbjni_SOURCES
  cxx/fbjni/*.cpp
  cxx/fbjni/detail/*.cpp
  cxx/lyra/*.cpp
)
add_library(fbjni SHARED
  ${fbjni_SOURCES}
)

target_compile_options(fbjni PRIVATE
  -fno-omit-frame-pointer
  -fexceptions
  -O3
  -Wall
  -std=c++11
  -frtti
  -ffunction-sections
  -DDISABLE_CPUCAP
  -DDISABLE_XPLAT
  -DNDEBUG
)

target_include_directories(fbjni PUBLIC
  cxx
)

target_link_libraries(fbjni
  android
  log
)
