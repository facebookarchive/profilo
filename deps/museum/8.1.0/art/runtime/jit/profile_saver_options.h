/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 * * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_JIT_PROFILE_SAVER_OPTIONS_H_
#define ART_RUNTIME_JIT_PROFILE_SAVER_OPTIONS_H_

#include <string>
#include <ostream>

namespace art {

struct ProfileSaverOptions {
 public:
  static constexpr uint32_t kMinSavePeriodMs = 40 * 1000;  // 40 seconds
  static constexpr uint32_t kSaveResolvedClassesDelayMs = 5 * 1000;  // 5 seconds
  // Minimum number of JIT samples during launch to mark a method as hot in the profile.
  static constexpr uint32_t kHotStartupMethodSamples = 1;
  static constexpr uint32_t kHotStartupMethodSamplesLowRam = 256;
  static constexpr uint32_t kMinMethodsToSave = 10;
  static constexpr uint32_t kMinClassesToSave = 10;
  static constexpr uint32_t kMinNotificationBeforeWake = 10;
  static constexpr uint32_t kMaxNotificationBeforeWake = 50;
  static constexpr uint32_t kHotStartupMethodSamplesNotSet = std::numeric_limits<uint32_t>::max();

  ProfileSaverOptions() :
    enabled_(false),
    min_save_period_ms_(kMinSavePeriodMs),
    save_resolved_classes_delay_ms_(kSaveResolvedClassesDelayMs),
    hot_startup_method_samples_(kHotStartupMethodSamplesNotSet),
    min_methods_to_save_(kMinMethodsToSave),
    min_classes_to_save_(kMinClassesToSave),
    min_notification_before_wake_(kMinNotificationBeforeWake),
    max_notification_before_wake_(kMaxNotificationBeforeWake),
    profile_path_(""),
    profile_boot_class_path_(false) {}

  ProfileSaverOptions(
      bool enabled,
      uint32_t min_save_period_ms,
      uint32_t save_resolved_classes_delay_ms,
      uint32_t hot_startup_method_samples,
      uint32_t min_methods_to_save,
      uint32_t min_classes_to_save,
      uint32_t min_notification_before_wake,
      uint32_t max_notification_before_wake,
      const std::string& profile_path,
      bool profile_boot_class_path)
  : enabled_(enabled),
    min_save_period_ms_(min_save_period_ms),
    save_resolved_classes_delay_ms_(save_resolved_classes_delay_ms),
    hot_startup_method_samples_(hot_startup_method_samples),
    min_methods_to_save_(min_methods_to_save),
    min_classes_to_save_(min_classes_to_save),
    min_notification_before_wake_(min_notification_before_wake),
    max_notification_before_wake_(max_notification_before_wake),
    profile_path_(profile_path),
    profile_boot_class_path_(profile_boot_class_path) {}

  bool IsEnabled() const {
    return enabled_;
  }
  void SetEnabled(bool enabled) {
    enabled_ = enabled;
  }

  uint32_t GetMinSavePeriodMs() const {
    return min_save_period_ms_;
  }
  uint32_t GetSaveResolvedClassesDelayMs() const {
    return save_resolved_classes_delay_ms_;
  }
  uint32_t GetHotStartupMethodSamples(bool is_low_ram) const {
    uint32_t ret = hot_startup_method_samples_;
    if (ret == kHotStartupMethodSamplesNotSet) {
      ret = is_low_ram ? kHotStartupMethodSamplesLowRam : kHotStartupMethodSamples;
    }
    return ret;
  }
  uint32_t GetMinMethodsToSave() const {
    return min_methods_to_save_;
  }
  uint32_t GetMinClassesToSave() const {
    return min_classes_to_save_;
  }
  uint32_t GetMinNotificationBeforeWake() const {
    return min_notification_before_wake_;
  }
  uint32_t GetMaxNotificationBeforeWake() const {
    return max_notification_before_wake_;
  }
  std::string GetProfilePath() const {
    return profile_path_;
  }
  bool GetProfileBootClassPath() const {
    return profile_boot_class_path_;
  }

  friend std::ostream & operator<<(std::ostream &os, const ProfileSaverOptions& pso) {
    os << "enabled_" << pso.enabled_
        << ", min_save_period_ms_" << pso.min_save_period_ms_
        << ", save_resolved_classes_delay_ms_" << pso.save_resolved_classes_delay_ms_
        << ", hot_startup_method_samples_" << pso.hot_startup_method_samples_
        << ", min_methods_to_save_" << pso.min_methods_to_save_
        << ", min_classes_to_save_" << pso.min_classes_to_save_
        << ", min_notification_before_wake_" << pso.min_notification_before_wake_
        << ", max_notification_before_wake_" << pso.max_notification_before_wake_
        << ", profile_boot_class_path_" << pso.profile_boot_class_path_;
    return os;
  }

  bool enabled_;
  uint32_t min_save_period_ms_;
  uint32_t save_resolved_classes_delay_ms_;
  // Do not access hot_startup_method_samples_ directly for reading since it may be set to the
  // placeholder default.
  uint32_t hot_startup_method_samples_;
  uint32_t min_methods_to_save_;
  uint32_t min_classes_to_save_;
  uint32_t min_notification_before_wake_;
  uint32_t max_notification_before_wake_;
  std::string profile_path_;
  bool profile_boot_class_path_;
};

}  // namespace art

#endif  // ART_RUNTIME_JIT_PROFILE_SAVER_OPTIONS_H_
