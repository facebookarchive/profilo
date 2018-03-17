// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#define ___PP_STRINGIFY(X) #X
#define PP_STRINGIFY(X) ___PP_STRINGIFY(X)
#define MUSEUM_HEADER(H) PP_STRINGIFY(museum/MUSEUM_VERSION_DOT/H)
