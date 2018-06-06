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

package com.facebook.profilo.util;

import static org.mockito.Matchers.anyInt;
import static org.powermock.api.mockito.PowerMockito.doAnswer;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.when;

import com.facebook.profilo.core.TraceEvents;
import org.junit.rules.MethodRule;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.powermock.reflect.Whitebox;

public class TraceEventsFakeRule implements MethodRule {

  public void enableProviders(int providers) {
    try {
      Whitebox.invokeMethod(TraceEvents.class, "enableProviders", providers);
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  public void disableProviders(int providers) {
    try {
      Whitebox.invokeMethod(TraceEvents.class, "disableProviders", providers);
    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  private static class StatementWithTraceEventsMock extends Statement {

    private Statement mParentStatement;
    private int mEnabledProviders;
    private final int[] mEnabledProviderCounts = new int[32];

    StatementWithTraceEventsMock(Statement parentStatement) {
      mParentStatement = parentStatement;
    }

    @Override
    public void evaluate() throws Throwable {
      setup();
      mParentStatement.evaluate();
    }

    private void setup() throws Exception {
      mockStatic(TraceEvents.class);

      doAnswer(
              new Answer<Void>() {
                @Override
                public Void answer(InvocationOnMock invocation) throws Throwable {
                  int providersToEnable = (int) invocation.getArguments()[0];
                  synchronized (StatementWithTraceEventsMock.this) {
                    for (int i = 0, p = 1; i < 32; i++, p <<= 1) {
                      if ((providersToEnable & p) == p) {
                        mEnabledProviderCounts[i]++;
                      }
                    }
                    mEnabledProviders |= providersToEnable;
                  }
                  return null;
                }
              })
          .when(TraceEvents.class, "enableProviders", anyInt());

      doAnswer(
              new Answer<Void>() {
                @Override
                public Void answer(InvocationOnMock invocation) throws Throwable {
                  int providersToDisable = (int) invocation.getArguments()[0];
                  synchronized (StatementWithTraceEventsMock.this) {
                    int disableProvidersMask = 0;
                    for (int i = 0, p = 1; i < 32; i++, p <<= 1) {
                      if ((providersToDisable & p) == p) {
                        mEnabledProviderCounts[i]--;
                        if (mEnabledProviderCounts[i] == 0) {
                          disableProvidersMask |= p;
                        }
                      }
                    }
                    mEnabledProviders &= ~disableProvidersMask;
                  }
                  return null;
                }
              })
          .when(TraceEvents.class, "disableProviders", anyInt());

      when(TraceEvents.isEnabled(anyInt())).thenAnswer(
          new Answer<Boolean>() {
            @Override
            public Boolean answer(InvocationOnMock invocation) throws Throwable {
              int providers = (int) invocation.getArguments()[0];
              synchronized (StatementWithTraceEventsMock.this) {
                return (mEnabledProviders & providers) == providers;
              }
            }
          }
      );

      when(TraceEvents.enabledMask(anyInt()))
          .thenAnswer(
              new Answer<Integer>() {
                @Override
                public Integer answer(InvocationOnMock invocation) throws Throwable {
                  int providers = (int) invocation.getArguments()[0];
                  synchronized (StatementWithTraceEventsMock.this) {
                    return mEnabledProviders & providers;
                  }
                }
              });
    }
  }

  @Override
  public Statement apply(Statement statement, FrameworkMethod frameworkMethod, Object o) {
    return new StatementWithTraceEventsMock(statement);
  }
}
