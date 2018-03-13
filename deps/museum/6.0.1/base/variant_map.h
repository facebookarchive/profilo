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

#ifndef ART_RUNTIME_BASE_VARIANT_MAP_H_
#define ART_RUNTIME_BASE_VARIANT_MAP_H_

#include <memory.h>
#include <map>
#include <utility>

namespace art {

//
// A variant map is a heterogenous, type safe key->value map. It allows
// for multiple different value types to be stored dynamically in the same map.
//
// It provides the following interface in a nutshell:
//
// struct VariantMap {
//   template <typename TValue>
//   TValue* Get(Key<T> key);  // null if the value was never set, otherwise the value.
//
//   template <typename TValue>
//   void Set(Key<T> key, TValue value);
// };
//
// Since the key is strongly typed at compile-time, it is impossible to accidentally
// read/write a value with a different type than the key at either compile-time or run-time.
//
// Do not use VariantMap/VariantMapKey directly. Instead subclass each of them and use
// the subclass, for example:
//
// template <typename TValue>
// struct FruitMapKey : VariantMapKey<TValue> {
//   FruitMapKey() {}
// };
//
// struct FruitMap : VariantMap<FruitMap, FruitMapKey> {
//   // This 'using' line is necessary to inherit the variadic constructor.
//   using VariantMap<FruitMap, FruitMapKey>::VariantMap;
//
//   // Make the next '4' usages of Key slightly shorter to type.
//   template <typename TValue>
//   using Key = FruitMapKey<TValue>;
//
//   static const Key<int> Apple;
//   static const Key<double> Orange;
//   static const Key<std::string> Banana;
// };
//
// const FruitMap::Key<int> FruitMap::Apple;
// const FruitMap::Key<double> FruitMap::Orange;
// const FruitMap::Key<std::string> Banana;
//
// See variant_map_test.cc for more examples.
//

// Implementation details for VariantMap.
namespace detail {
  // Allocate a unique counter value each time it's called.
  struct VariantMapKeyCounterAllocator {
    static size_t AllocateCounter() {
      static size_t counter = 0;
      counter++;

      return counter;
    }
  };

  // Type-erased version of VariantMapKey<T>
  struct VariantMapKeyRaw {
    // TODO: this may need to call a virtual function to support string comparisons
    bool operator<(const VariantMapKeyRaw& other) const {
      return key_counter_ < other.key_counter_;
    }

    // The following functions need to be virtual since we don't know the compile-time type anymore:

    // Clone the key, creating a copy of the contents.
    virtual VariantMapKeyRaw* Clone() const = 0;

    // Delete a value whose runtime type is that of the non-erased key's TValue.
    virtual void ValueDelete(void* value) const = 0;

    // Clone a value whose runtime type is that of the non-erased key's TValue.
    virtual void* ValueClone(void* value) const = 0;

    // Compare one key to another (same as operator<).
    virtual bool Compare(const VariantMapKeyRaw* other) const {
      if (other == nullptr) {
        return false;
      }
      return key_counter_ < other->key_counter_;
    }

    virtual ~VariantMapKeyRaw() {}

   protected:
    VariantMapKeyRaw()
        : key_counter_(VariantMapKeyCounterAllocator::AllocateCounter()) {}
    // explicit VariantMapKeyRaw(size_t counter)
    //     : key_counter_(counter) {}

    size_t GetCounter() const {
      return key_counter_;
    }

   protected:
    // Avoid the object slicing problem; use Clone() instead.
    VariantMapKeyRaw(const VariantMapKeyRaw&) = default;
    VariantMapKeyRaw(VariantMapKeyRaw&&) = default;

   private:
    size_t key_counter_;  // Runtime type ID. Unique each time a new type is reified.
  };
}  // namespace detail

// The base type for keys used by the VariantMap. Users must subclass this type.
template <typename TValue>
struct VariantMapKey : detail::VariantMapKeyRaw {
  // Instantiate a default value for this key. If an explicit default value was provided
  // then that is used. Otherwise, the default value for the type TValue{} is returned.
  TValue CreateDefaultValue() const {
    if (default_value_ == nullptr) {
      return TValue{};  // NOLINT [readability/braces] [4]
    } else {
      return TValue(*default_value_);
    }
  }

 protected:
  // explicit VariantMapKey(size_t counter) : detail::VariantMapKeyRaw(counter) {}
  explicit VariantMapKey(const TValue& default_value)
    : default_value_(std::make_shared<TValue>(default_value)) {}
  explicit VariantMapKey(TValue&& default_value)
    : default_value_(std::make_shared<TValue>(default_value)) {}
  VariantMapKey() {}
  virtual ~VariantMapKey() {}

 private:
  virtual VariantMapKeyRaw* Clone() const {
    return new VariantMapKey<TValue>(*this);
  }

  virtual void* ValueClone(void* value) const {
    if (value == nullptr) {
      return nullptr;
    }

    TValue* strong_value = reinterpret_cast<TValue*>(value);
    return new TValue(*strong_value);
  }

  virtual void ValueDelete(void* value) const {
    if (value == nullptr) {
      return;
    }

    // Smartly invoke the proper delete/delete[]/etc
    const std::default_delete<TValue> deleter = std::default_delete<TValue>();
    deleter(reinterpret_cast<TValue*>(value));
  }

  VariantMapKey(const VariantMapKey&) = default;
  VariantMapKey(VariantMapKey&&) = default;

  template <typename Base, template <typename TV> class TKey> friend struct VariantMap;

  // Store a prototype of the key's default value, for usage with VariantMap::GetOrDefault
  std::shared_ptr<TValue> default_value_;
};

// Implementation details for a stringified VariantMapStringKey.
namespace detail {
  struct VariantMapStringKeyRegistry {
    // TODO
  };
}  // namespace detail

// Alternative base type for all keys used by VariantMap, supports runtime strings as the name.
template <typename TValue>
struct VariantMapStringKey : VariantMapKey<TValue> {
  explicit VariantMapStringKey(const char* name)
      :   // VariantMapKey(/*std::hash<std::string>()(name)*/),
        name_(name) {
  }

 private:
  const char* name_;
};

// A variant map allows type-safe heteregeneous key->value mappings.
// All possible key types must be specified at compile-time. Values may be added/removed
// at runtime.
template <typename Base, template <typename TV> class TKey>
struct VariantMap {
  // Allow users of this static interface to use the key type.
  template <typename TValue>
  using Key = TKey<TValue>;

  // Look up the value from the key. The pointer becomes invalid if this key is overwritten/removed.
  // A null value is returned only when the key does not exist in this map.
  template <typename TValue>
  const TValue* Get(const TKey<TValue>& key) const {
    return GetValuePtr(key);
  }

  // Look up the value from the key. The pointer becomes invalid if this key is overwritten/removed.
  // A null value is returned only when the key does not exist in this map.
  template <typename TValue>
  TValue* Get(const TKey<TValue>& key) {
    return GetValuePtr(key);
  }

  // Lookup the value from the key. If it was not set in the map, return the default value.
  // The default value is either the key's default, or TValue{} if the key doesn't have a default.
  template <typename TValue>
  TValue GetOrDefault(const TKey<TValue>& key) const {
    auto* ptr = Get(key);
    return (ptr == nullptr) ? key.CreateDefaultValue() : *ptr;
  }

 private:
  // TODO: move to detail, or make it more generic like a ScopeGuard(function)
  template <typename TValue>
  struct ScopedRemove {
    ScopedRemove(VariantMap& map, const TKey<TValue>& key) : map_(map), key_(key) {}
    ~ScopedRemove() {
      map_.Remove(key_);
    }

    VariantMap& map_;
    const TKey<TValue>& key_;
  };

 public:
  // Release the value from the key. If it was not set in the map, returns the default value.
  // If the key was set, it is removed as a side effect.
  template <typename TValue>
  TValue ReleaseOrDefault(const TKey<TValue>& key) {
    ScopedRemove<TValue> remove_on_return(*this, key);

    TValue* ptr = Get(key);
    if (ptr != nullptr) {
      return std::move(*ptr);
    } else {
      TValue default_value = key.CreateDefaultValue();
      return std::move(default_value);
    }
  }

  // See if a value is stored for this key.
  template <typename TValue>
  bool Exists(const TKey<TValue>& key) const {
    return GetKeyValueIterator(key) != storage_map_.end();
  }

  // Set a value for a given key, overwriting the previous value if any.
  template <typename TValue>
  void Set(const TKey<TValue>& key, const TValue& value) {
    // Clone the value first, to protect against &value == GetValuePtr(key).
    auto* new_value = new TValue(value);

    Remove(key);
    storage_map_.insert({{key.Clone(), new_value}});
  }

  // Set a value for a given key, only if there was no previous value before.
  // Returns true if the value was set, false if a previous value existed.
  template <typename TValue>
  bool SetIfMissing(const TKey<TValue>& key, const TValue& value) {
    TValue* ptr = Get(key);
    if (ptr == nullptr) {
      Set(key, value);
      return true;
    }
    return false;
  }

  // Remove the value for a given key, or a no-op if there was no previously set value.
  template <typename TValue>
  void Remove(const TKey<TValue>& key) {
    StaticAssertKeyType<TValue>();

    auto&& it = GetKeyValueIterator(key);
    if (it != storage_map_.end()) {
      key.ValueDelete(it->second);
      delete it->first;
      storage_map_.erase(it);
    }
  }

  // Remove all key/value pairs.
  void Clear() {
    DeleteStoredValues();
    storage_map_.clear();
  }

  // How many key/value pairs are stored in this map.
  size_t Size() const {
    return storage_map_.size();
  }

  // Construct an empty map.
  explicit VariantMap() {}

  template <typename ... TKeyValue>
  explicit VariantMap(const TKeyValue& ... key_value_list) {
    static_assert(sizeof...(TKeyValue) % 2 == 0, "Must be an even number of key/value elements");
    InitializeParameters(key_value_list...);
  }

  // Create a new map from an existing map, copying all the key/value pairs.
  VariantMap(const VariantMap& other) {
    operator=(other);
  }

  // Copy the key/value pairs from the other map into this one. Existing key/values are cleared.
  VariantMap& operator=(const VariantMap& other) {
    if (this == &other) {
      return *this;
    }

    Clear();

    for (auto&& kv_pair : other.storage_map_) {
      const detail::VariantMapKeyRaw* raw_key_other = kv_pair.first;
      void* value = kv_pair.second;

      detail::VariantMapKeyRaw* cloned_raw_key = raw_key_other->Clone();
      void* cloned_value = raw_key_other->ValueClone(value);

      storage_map_.insert({{ cloned_raw_key, cloned_value }});
    }

    return *this;
  }

  // Create a new map by moving an existing map into this one. The other map becomes empty.
  VariantMap(VariantMap&& other) {
    operator=(std::forward<VariantMap>(other));
  }

  // Move the existing map's key/value pairs into this one. The other map becomes empty.
  VariantMap& operator=(VariantMap&& other) {
    if (this != &other) {
      Clear();
      storage_map_.swap(other.storage_map_);
      other.storage_map_.clear();
    }
    return *this;
  }

  ~VariantMap() {
    DeleteStoredValues();
  }

 private:
  void InitializeParameters() {}

  template <typename TK, typename TValue, typename ... Rest>
  void InitializeParameters(const TK& key, const TValue& value, const Rest& ... rest) {
    static_assert(
        std::is_same<TK, TKey<TValue>>::value, "The 0th/2nd/4th/etc parameters must be a key");

    const TKey<TValue>& key_refined = key;

    Set(key_refined, value);
    InitializeParameters(rest...);
  }

  // Custom key comparator for std::map, needed since we are storing raw pointers as the keys.
  struct KeyComparator {
    bool operator()(const detail::VariantMapKeyRaw* lhs,
                    const detail::VariantMapKeyRaw* rhs) const {
      if (lhs == nullptr) {
        return lhs != rhs;
      }

      return lhs->Compare(rhs);
    }
  };

  // Map of key pointers to value pointers. Pointers are never null.
  using StorageMap = std::map<const detail::VariantMapKeyRaw*, void*, KeyComparator>;

  template <typename TValue>
  typename StorageMap::iterator GetKeyValueIterator(const TKey<TValue>& key) {
    StaticAssertKeyType<TValue>();

    const TKey<TValue>* key_ptr = &key;
    const detail::VariantMapKeyRaw* raw_ptr = key_ptr;
    return storage_map_.find(raw_ptr);
  }

  template <typename TValue>
  typename StorageMap::const_iterator GetKeyValueIterator(const TKey<TValue>& key) const {
    StaticAssertKeyType<TValue>();

    const TKey<TValue>* key_ptr = &key;
    const detail::VariantMapKeyRaw* raw_ptr = key_ptr;
    return storage_map_.find(raw_ptr);
  }

  template <typename TValue>
  TValue* GetValuePtr(const TKey<TValue>& key) {
    return const_cast<TValue*>(GetValueConstPtr(key));
  }

  template <typename TValue>
  const TValue* GetValuePtr(const TKey<TValue>& key) const {
    return GetValueConstPtr(key);
  }

  template <typename TValue>
  const TValue* GetValueConstPtr(const TKey<TValue>& key) const {
    auto&& it = GetKeyValueIterator(key);
    if (it == storage_map_.end()) {
      return nullptr;
    }

    return reinterpret_cast<const TValue*>(it->second);
  }

  template <typename TValue>
  static void StaticAssertKeyType() {
    static_assert(std::is_base_of<VariantMapKey<TValue>, TKey<TValue>>::value,
                  "The provided key type (TKey) must be a subclass of VariantMapKey");
  }

  void DeleteStoredValues() {
    for (auto&& kv_pair : storage_map_) {
      kv_pair.first->ValueDelete(kv_pair.second);
      delete kv_pair.first;
    }
  }

  StorageMap storage_map_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_VARIANT_MAP_H_
