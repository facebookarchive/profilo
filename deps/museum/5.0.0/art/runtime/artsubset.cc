// Copyright 2004-present Facebook. All Rights Reserved.

// This shitty hack renames the ART namespace so that different ART
// symbols don't collide.
#include "mirror/class.h"
#include "artsubset.h"

namespace facebook {

namespace {

using namespace art::mirror;

ArtMethod* FixedFindInterfaceMethod(Class* c, const DexCache* cache, uint32_t n) {
  return c->FindInterfaceMethod(cache, n);
}

ArtMethod *FixedFindVirtualMethod(Class* c, const DexCache* cache, uint32_t n) {
  return c->FindVirtualMethod(cache, n);
}

}

void*
getFixedArtClassFindInterfaceMethod()
{
  return reinterpret_cast<void*>(&FixedFindInterfaceMethod);
}

void*
getFixedArtClassFindDeclaredVirtualMethod()
{
  return reinterpret_cast<void*>(&FixedFindVirtualMethod);
}

void*
getFixedArtClassFindVirtualMethod()
{
  return reinterpret_cast<void*>(&FixedFindVirtualMethod);
}

}
