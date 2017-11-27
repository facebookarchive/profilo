// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.util;

import static org.mockito.Matchers.anyInt;
import static org.powermock.api.mockito.PowerMockito.doAnswer;
import static org.powermock.api.mockito.PowerMockito.mockStatic;
import static org.powermock.api.mockito.PowerMockito.when;

import com.facebook.loom.core.TraceEvents;
import org.junit.rules.MethodRule;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

public class TraceEventsFakeRule implements MethodRule {

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
