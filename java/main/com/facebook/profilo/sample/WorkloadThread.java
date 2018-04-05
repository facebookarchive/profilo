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

package com.facebook.profilo.sample;

import com.facebook.profilo.core.TraceControl;
import java.util.Random;

public class WorkloadThread extends Thread {

  public WorkloadThread(String name) {
    super(name);
  }

  @Override
  public void run() {
    Random rand = new Random();
    rand.setSeed(System.nanoTime());
    float functionSelector;
    while (true) {
      while (!TraceControl.get().isInsideTrace()) {
        try {
          Thread.sleep(500, 0);
        } catch (InterruptedException e) {
          // OK to just ignore exception
        }
      }
      while (TraceControl.get().isInsideTrace()) {
        functionSelector = rand.nextFloat();
        if (functionSelector < 0.20) {
          this.path_1();
        } else if (functionSelector < 0.50) {
          this.path_2();
        } else {
          this.path_3();
        }
      }
    }
  }

  private int path_1() {
    Random rand = new Random();
    rand.setSeed(System.nanoTime());
    float functionSelector = rand.nextFloat();

    int i = 0;
    while (i < 5000000) {
      i++; // Just do some idle work
    }

    try {
      Thread.sleep(1);
    } catch (InterruptedException e) {
      // OK to just ignore exception
    }

    if (functionSelector < 0.40) {
      return path_2();
    } else if (functionSelector < 0.80) {
      return path_3();
    } else {
      return 0;
    }
  }

  private int path_2() {
    Random rand = new Random();
    rand.setSeed(System.nanoTime());
    float functionSelector = rand.nextFloat();

    int i = 0;
    while (i < 5000000) {
      i++; // Just do some idle work
    }

    try {
      Thread.sleep(1);
    } catch (InterruptedException e) {
      // OK to just ignore exception
    }

    if (functionSelector < 0.40) {
      return path_1();
    } else if (functionSelector < 0.80) {
      return path_3();
    } else {
      return 0;
    }
  }

  private int path_3() {
    Random rand = new Random();
    rand.setSeed(System.nanoTime());
    float functionSelector = rand.nextFloat();

    int i = 0;
    while (i < 5000000) {
      i++; // Just do some idle work
    }

    try {
      Thread.sleep(1);
    } catch (InterruptedException e) {
      // OK to just ignore exception
    }

    if (functionSelector < 0.40) {
      return path_1();
    } else if (functionSelector < 0.80) {
      return path_2();
    } else {
      return 0;
    }
  }
}
