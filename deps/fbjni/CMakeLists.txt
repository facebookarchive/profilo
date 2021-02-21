# Copyright (c) Facebook, Inc. and its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.6.0)
project(fbjni CXX)

include(GNUInstallDirs)

set(FBJNI_COMPILE_OPTIONS
  -Wall
  -std=c++14
  -pthread
  -fno-omit-frame-pointer
  -fexceptions
  -frtti
  -ffunction-sections
  -O3
  -DNDEBUG
)

file(GLOB fbjni_SOURCES
  cxx/fbjni/*.cpp
  cxx/fbjni/detail/*.cpp
  cxx/lyra/*.cpp
)

add_library(fbjni SHARED ${fbjni_SOURCES})
set_target_properties(fbjni PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_compile_options(fbjni PRIVATE ${FBJNI_COMPILE_OPTIONS})
target_compile_options(fbjni PRIVATE -DBUILDING_FBJNI)

target_include_directories(fbjni BEFORE
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxx>
)

if (NOT ANDROID_ABI)
  if (NOT JAVA_HOME)
    message(FATAL_ERROR
      "fbjni requires JAVA_HOME to be defined for non-Android builds.")
  endif()
  target_include_directories(fbjni PUBLIC ${JAVA_HOME}/include)
  if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_include_directories(fbjni PUBLIC ${JAVA_HOME}/include/linux)
  endif()
  if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
    target_include_directories(fbjni PUBLIC ${JAVA_HOME}/include/darwin)
  endif()
  # win32/jni_md.h typedefs jint as long, because apparently
  # long is 32 bits on Windows.  This breaks a lot of stuff.
  # It could probably be fixed, but for now just require
  # building with Android's jni.h on Windows, which doesn't do this.
  # if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  #   target_include_directories(fbjni PUBLIC ${JAVA_HOME}/include/win32)
  # endif()

  if(NOT FBJNI_SKIP_TESTS)
    enable_testing()
    add_subdirectory(test/jni)

    find_library(GTEST_LIB gtest)
    if(NOT GTEST_LIB)
      # Download and unpack googletest at configure time
      configure_file(googletest-CMakeLists.txt.in googletest-download/CMakeLists.txt)
      execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
      if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
      endif()
      execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
      if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
      endif()

      # Add googletest directly to our build. This defines
      # the gtest and gtest_main targets.
      add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                      ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                      EXCLUDE_FROM_ALL)
    endif()
  endif()
endif()

if (ANDROID_ABI)
  target_link_libraries(fbjni
    android
    log
  )
endif()

install(TARGETS fbjni EXPORT fbjniLibraryConfig
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}) #For windows

install(EXPORT fbjniLibraryConfig DESTINATION share/cmake/fbjni
  FILE fbjniLibraryConfig.cmake)

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:fbjni> DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
    install(TARGETS fbjni DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
