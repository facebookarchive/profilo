/*
 * Copyright (C) 2017 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <string.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include "private/bionic_lock.h"

class CachedProperty {
 public:
  CachedProperty(const char* property_name)
    : property_name_(property_name),
      prop_info_(nullptr),
      cached_area_serial_(0),
      cached_property_serial_(0) {
    cached_value_[0] = '\0';
  }

  const char* Get() {
    lock_.lock();

    // Do we have a `struct prop_info` yet?
    if (prop_info_ == nullptr) {
      // `__system_property_find` is expensive, so only retry if a property
      // has been created since last time we checked.
      uint32_t property_area_serial = __system_property_area_serial();
      if (property_area_serial != cached_area_serial_) {
        prop_info_ = __system_property_find(property_name_);
        cached_area_serial_ = property_area_serial;
      }
    }

    if (prop_info_ != nullptr) {
      // Only bother re-reading the property if it's actually changed since last time.
      uint32_t property_serial = __system_property_serial(prop_info_);
      if (property_serial != cached_property_serial_) {
        __system_property_read_callback(prop_info_, &CachedProperty::Callback, this);
      }
    }

    lock_.unlock();
    return cached_value_;
  }

 private:
  Lock lock_;
  const char* property_name_;
  const prop_info* prop_info_;
  uint32_t cached_area_serial_;
  uint32_t cached_property_serial_;
  char cached_value_[PROP_VALUE_MAX];

  static void Callback(void* data, const char*, const char* value, uint32_t serial) {
    CachedProperty* instance = reinterpret_cast<CachedProperty*>(data);
    instance->cached_property_serial_ = serial;
    strcpy(instance->cached_value_, value);
  }
};
