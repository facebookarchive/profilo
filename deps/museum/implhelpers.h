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

#include <museum/libart.h>

namespace facebook {

void registerSymbolLookup(void*(*lookup)());

namespace art_internals {
struct SymbolLookupRegistration {
  SymbolLookupRegistration(void*(*lookup)()) {
    registerSymbolLookup(lookup);
  }
};

} }

#define TAI_6(T1, T2, T3, T4, T5, T6)       T1 p1, T2 p2, T3 p3, T4 p4, T5 p5, T6 p6
#define TAI_5(T1, T2, T3, T4, T5)           T1 p1, T2 p2, T3 p3, T4 p4, T5 p5
#define TAI_4(T1, T2, T3, T4)               T1 p1, T2 p2, T3 p3, T4 p4
#define TAI_3(T1, T2, T3)                   T1 p1, T2 p2, T3 p3
#define TAI_2(T1, T2)                       T1 p1, T2 p2
#define TAI_1(T1)                           T1 p1
#define TAI_0()                           /**/
#define TAI_N(unused, a, b, c, d, e, f, count, ...) TAI_ ## count
#define TAI(...) TAI_N(, ## __VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__)

#define PARM_NAMES_6                        p1, p2, p3, p4, p5, p6
#define PARM_NAMES_5                        p1, p2, p3, p4, p5
#define PARM_NAMES_4                        p1, p2, p3, p4
#define PARM_NAMES_3                        p1, p2, p3
#define PARM_NAMES_2                        p1, p2
#define PARM_NAMES_1                        p1
#define PARM_NAMES_0                      /**/
#define PARM_NAMES_N(unused, a, b, c, d, e, f, count, ...) PARM_NAMES_ ## count
#define PARM_NAMES(...) PARM_NAMES_N(, ## __VA_ARGS__, 6, 5, 4, 3, 2, 1, 0)

#define COMMA_IF_ARGS_1                     ,
#define COMMA_IF_ARGS_0                   /**/
#define COMMA_IF_ARGS_N(unused, a, b, c, d, e, f, count, ...) COMMA_IF_ARGS_ ## count
#define COMMA_IF_ARGS(...) COMMA_IF_ARGS_N(, ## __VA_ARGS__, 1, 1, 1, 1, 1, 1, 0)

#define SYM_LOOKUP(MANGLED)                                                                                     \
namespace {                                                                                                     \
  template<typename T>                                                                                          \
  T* lookup_ ## MANGLED () {                                                                                    \
    static auto const method = ::facebook::libart().get_symbol< T >(#MANGLED);                                  \
    return method;                                                                                              \
  }                                                                                                             \
  ::facebook::art_internals::SymbolLookupRegistration slr_ ## MANGLED ((void*(*)())&lookup_ ## MANGLED );       \
}

#define __IMPL(MANGLED, DECL_RET, RET, CLASS, NAME, CV, RETURN, ...)                                            \
SYM_LOOKUP(MANGLED)                                                                                             \
DECL_RET CLASS :: NAME ( TAI(__VA_ARGS__) ) CV {                                                                \
  RETURN lookup_ ## MANGLED < RET ( CLASS CV *, ## __VA_ARGS__ ) > ()                                           \
    ( this COMMA_IF_ARGS(__VA_ARGS__) PARM_NAMES(__VA_ARGS__) );                                                \
}

#define IMPL(MANGLED, RET, CLASS, NAME, ...)        __IMPL(MANGLED, RET , RET , CLASS,  NAME , /**/, return, ## __VA_ARGS__)
#define IMPL_CV(MANGLED, RET, CLASS, NAME, CV, ...) __IMPL(MANGLED, RET , RET , CLASS,  NAME , CV  , return, ## __VA_ARGS__)
#define IMPL_CTOR(MANGLED, CLASS, ...)              __IMPL(MANGLED, /**/, void, CLASS,  CLASS, /**/, /**/  , ## __VA_ARGS__)
#define IMPL_DTOR(MANGLED, CLASS)                   __IMPL(MANGLED, /**/, void, CLASS, ~CLASS, /**/, /**/  )

#define OBJ_IMPL(MANGLED, TYPE, CLASS, NAME)                                                                    \
SYM_LOOKUP(MANGLED)                                                                                             \
TYPE CLASS :: NAME {                                                                                            \
  return *lookup_ ## MANGLED < TYPE > ();                                                                       \
}

#define STANDALONE_IMPL(MANGLED, RET, NAME, ...)                                                                \
SYM_LOOKUP(MANGLED)                                                                                             \
RET NAME ( TAI(__VA_ARGS__) ) {                                                                                 \
  return lookup_ ## MANGLED < RET ( __VA_ARGS__ ) > ()( PARM_NAMES(__VA_ARGS__) );                              \
}
