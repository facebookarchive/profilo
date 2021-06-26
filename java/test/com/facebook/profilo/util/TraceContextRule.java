/**
 * Copyright 2004-present, Facebook, Inc.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.facebook.profilo.util;

import com.facebook.profilo.ipc.TraceContext;
import com.facebook.profilo.mmapbuf.Buffer;
import com.facebook.profilo.mmapbuf.MmapBufferManager;
import java.io.File;
import java.util.ArrayList;
import org.junit.rules.MethodRule;
import org.junit.runners.model.FrameworkMethod;
import org.junit.runners.model.Statement;

public class TraceContextRule implements MethodRule {

  public TraceContextRule(int providers) {
    mProviders = providers;
  }

  private final int mProviders;
  private final MmapBufferManager mBufferManager = new MmapBufferManager(new File("."));
  private ArrayList<Buffer> mBuffers = new ArrayList<>();
  private TraceContext mContext = null;

  private void newContext() {
    mContext = new TraceContext();
    mContext.enabledProviders = mProviders;
    mContext.mainBuffer = newBuffer();
    mContext.buffers = new Buffer[] {mContext.mainBuffer};
  }

  public Buffer newBuffer() {
    Buffer buffer = mBufferManager.allocateBuffer(1000, false);
    mBuffers.add(buffer);
    return buffer;
  }

  private void destroyBuffers() {
    for (Buffer buffer : mBuffers) {
      mBufferManager.deallocateBuffer(buffer);
    }
    mBuffers.clear();
  }

  public TraceContext getContext() {
    return mContext;
  }

  @Override
  public Statement apply(Statement statement, FrameworkMethod frameworkMethod, Object o) {
    return new StatementWithTraceContext(statement);
  }

  private class StatementWithTraceContext extends Statement {
    private Statement mParentStatement;

    StatementWithTraceContext(Statement parentStatement) {
      mParentStatement = parentStatement;
    }

    @Override
    public void evaluate() throws Throwable {
      newContext();
      mParentStatement.evaluate();

      destroyBuffers();
      mContext = null;
    }
  }
}
