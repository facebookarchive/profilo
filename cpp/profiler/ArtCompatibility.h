// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

namespace facebook {
namespace profilo {
namespace artcompat {

#if defined(ART_VERSION_5_1_1)
void registerNatives_5_1_1();
#endif
#if defined(ART_VERSION_6_0_1)
void registerNatives_6_0_1();
#endif
#if defined(ART_VERSION_7_0_0)
void registerNatives_7_0_0();
#endif

} // artcompat
} // profilo
} // facebook
