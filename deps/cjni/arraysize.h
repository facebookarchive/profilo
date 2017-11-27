// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

/**
 * Macro determining size of static array.
 */
#define ARRAY_SIZE(ar) (sizeof(ar) / sizeof((ar)[0]))
