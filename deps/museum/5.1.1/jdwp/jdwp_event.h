/*
 * Copyright (C) 2008 The Android Open Source Project
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
/*
 * Handle registration of events, and debugger event notification.
 */
#ifndef ART_RUNTIME_JDWP_JDWP_EVENT_H_
#define ART_RUNTIME_JDWP_JDWP_EVENT_H_

#include "jdwp/jdwp.h"
#include "jdwp/jdwp_constants.h"
#include "jdwp/jdwp_expand_buf.h"

namespace art {

namespace JDWP {

/*
 * Event modifiers.  A JdwpEvent may have zero or more of these.
 */
union JdwpEventMod {
  JdwpModKind modKind;
  struct {
    JdwpModKind modKind;
    int         count;
  } count;
  struct {
    JdwpModKind modKind;
    uint32_t          exprId;
  } conditional;
  struct {
    JdwpModKind modKind;
    ObjectId    threadId;
  } threadOnly;
  struct {
    JdwpModKind modKind;
    RefTypeId   refTypeId;
  } classOnly;
  struct {
    JdwpModKind modKind;
    char*       classPattern;
  } classMatch;
  struct {
    JdwpModKind modKind;
    char*       classPattern;
  } classExclude;
  struct {
    JdwpModKind modKind;
    JdwpLocation loc;
  } locationOnly;
  struct {
    JdwpModKind modKind;
    uint8_t          caught;
    uint8_t          uncaught;
    RefTypeId   refTypeId;
  } exceptionOnly;
  struct {
    JdwpModKind modKind;
    RefTypeId   refTypeId;
    FieldId     fieldId;
  } fieldOnly;
  struct {
    JdwpModKind modKind;
    ObjectId    threadId;
    int         size;           /* JdwpStepSize */
    int         depth;          /* JdwpStepDepth */
  } step;
  struct {
    JdwpModKind modKind;
    ObjectId    objectId;
  } instanceOnly;
};

/*
 * One of these for every registered event.
 *
 * We over-allocate the struct to hold the modifiers.
 */
struct JdwpEvent {
  JdwpEvent* prev;           /* linked list */
  JdwpEvent* next;

  JdwpEventKind eventKind;      /* what kind of event is this? */
  JdwpSuspendPolicy suspend_policy;  /* suspend all, none, or self? */
  int modCount;       /* #of entries in mods[] */
  uint32_t requestId;      /* serial#, reported to debugger */

  JdwpEventMod mods[1];        /* MUST be last field in struct */
};

/*
 * Allocate an event structure with enough space.
 */
JdwpEvent* EventAlloc(int numMods);
void EventFree(JdwpEvent* pEvent);

}  // namespace JDWP

}  // namespace art

#endif  // ART_RUNTIME_JDWP_JDWP_EVENT_H_
