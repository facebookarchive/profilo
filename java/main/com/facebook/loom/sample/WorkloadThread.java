// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.sample;

import com.facebook.loom.core.TraceControl;
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
      Thread.sleep(250);
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
      Thread.sleep(250);
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
      Thread.sleep(250);
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
