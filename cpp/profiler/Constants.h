// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

/**
  * The number of method entries in the stack trace per a single stack trace to report
  */
#define MAX_STACK_DEPTH 255

/**
  * The maximum number of frames to track stack traces
  */
#define MAX_STACKS_COUNT 24

/**
  * Number of full stacks upon which should flush stacks to the Loom buffer
  */
#define FLUSH_STACKS_COUNT 8
