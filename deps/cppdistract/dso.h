// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <initializer_list>

namespace facebook { namespace cppdistract {

/**
 * RAII object wrapping dlopen() and dlsym()
 */
class dso {
public:
    /**
     * Opens the named library, using dlopen(2).
     *
     * Throws std::runtime_error if the library fails to load.
     */
    explicit dso(char const* name);
    ~dso();

    /**
     * Returns the dlopen handle to this library.
     *
     * NOTA BENE: This is explicitly a _handle_. It is not necessarily the address of the library.
     */
    void* get_handle() const;

    /**
     * Returns the named symbol exported by this library.
     *
     * Throws std::runtime_error if not found.
     */
    template <typename T>
    T* get_symbol(char const* name) const {
      return get_symbol<T>({ name });
    }

    /**
     * Takes a list of possible names, returns the first symbol among them that is found.
     *
     * Throws std::runtime_error if none are found.
     */
    template<typename T>
    T* get_symbol(std::initializer_list<char const*> const& names) const {
      return reinterpret_cast<T*>(get_symbol_internal(names));
    }

private:
    void* const handle_;

    void* get_symbol_internal(std::initializer_list<char const*> const& names) const;
};

} } // namespace facebook::cppdistract
