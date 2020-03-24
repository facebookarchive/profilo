/**
 * Copyright 2018-present, Facebook, Inc.
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

package com.facebook.profilo.ipc;

import com.facebook.profilo.ipc.IProfiloMultiProcessTraceService;
import com.facebook.profilo.ipc.TraceContext;

interface IProfiloMultiProcessTraceListener {

  /**
   * Called after the IProfiloMultiProcessTraceService from another
   * process has received the listener
   */
  void onReceive(IProfiloMultiProcessTraceService service);

  void onTraceStart(in TraceContext context);

  void onTraceStop(in TraceContext context);

  void onTraceAbort(in TraceContext context);

  void waitForTraceClose(in long traceId);
}
