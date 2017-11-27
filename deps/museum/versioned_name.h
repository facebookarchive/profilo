// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#define PASTER(x,y) x ## _ ## y
#define EVALUATOR(x,y) PASTER(x,y)

/**
 * Generates a string with "_<ART_VERSION>" tacked on, where ART_VERSION should be already
 * defined - usually set by preprocessor flags in a version-specific target's BUCK file.
 */
#define VERSIONED_NAME(name) EVALUATOR(name, ART_VERSION)
