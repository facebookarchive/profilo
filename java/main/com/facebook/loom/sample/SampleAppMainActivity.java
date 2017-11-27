// Copyright 2004-present Facebook. All Rights Reserved.

package com.facebook.loom.sample;

import android.app.Activity;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.ToggleButton;
import com.facebook.loom.BuildConfig;
import com.facebook.loom.controllers.manual.ManualTraceController;
import com.facebook.loom.core.LoomConstants;
import com.facebook.loom.core.TraceControl;
import com.facebook.loom.core.TraceController;
import com.facebook.loom.core.TraceOrchestrator;
import com.facebook.loom.logger.Trace;
import com.facebook.loom.provider.atrace.SystraceProvider;
import com.facebook.loom.provider.stacktrace.StackFrameThread;
import com.facebook.loom.provider.systemcounters.SystemCounterThread;
import com.facebook.loom.provider.threadmetadata.ThreadMetadataProvider;
import com.facebook.loom.provider.yarn.PerfEventsProvider;
import com.facebook.soloader.SoLoader;
import java.io.IOException;

public class SampleAppMainActivity extends Activity {

  private Thread mWorkerThread;
  private ToggleButton mTracingButton;
  private ProgressBar mProgressBar;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    initializeLoom();

    createLayout();

    mWorkerThread = new Thread("busy-thread") {
      @Override
      public void run() {
        int foo = 0xface;
        while (true) {
          while (!TraceControl.get().isInsideTrace()) {
            try {
              Thread.sleep(500, 0);
            } catch (InterruptedException e) {
            }
          }
          while (TraceControl.get().isInsideTrace()) {
            foo *= 0xb00c;
          }
        }
      }
    };

    mWorkerThread.start();
    mTracingButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        if (mTracingButton.isChecked()) {
          TraceControl.get().startTrace(
              LoomConstants.TRIGGER_MANUAL,
              Trace.FLAG_MANUAL,
              null,
              0);
          mProgressBar.setVisibility(View.VISIBLE);
        } else {
          TraceControl.get().stopTrace(
              LoomConstants.TRIGGER_MANUAL,
              null,
              0);
          mProgressBar.setVisibility(View.INVISIBLE);
        }
      }
    });
  }

  private void initializeLoom() {
    try {
      SoLoader.init(this.getApplicationContext(), 0);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }

    SparseArray<TraceController> controllers = new SparseArray<>(1);
    controllers.put(LoomConstants.TRIGGER_MANUAL, new ManualTraceController());

    TraceOrchestrator.initialize(
        this.getApplicationContext(),
        null,
        TraceOrchestrator.MAIN_PROCESS_NAME,
        /* isMainProcess */ true,
        calculateProviders(),
        controllers);
  }

  private void createLayout() {
    RelativeLayout layout = new RelativeLayout(this);
    ToggleButton traceButton = new ToggleButton(this);
    ProgressBar progressBar = new ProgressBar(this);

    final int BUTTON_ID = 0x100;

    traceButton.setTextOff("Start tracing");
    traceButton.setTextOn("Stop tracing");
    traceButton.setChecked(false); // force the string to update
    traceButton.setId(0x100);

    progressBar.setIndeterminate(true);
    progressBar.setVisibility(View.INVISIBLE);

    RelativeLayout.LayoutParams buttonParams = new RelativeLayout.LayoutParams(
        ViewGroup.MarginLayoutParams.WRAP_CONTENT,
        ViewGroup.MarginLayoutParams.WRAP_CONTENT);
    buttonParams.addRule(RelativeLayout.CENTER_IN_PARENT, 1);

    layout.addView(traceButton, buttonParams);

    RelativeLayout.LayoutParams spinnerParams = new RelativeLayout.LayoutParams(
        ViewGroup.MarginLayoutParams.WRAP_CONTENT,
        ViewGroup.MarginLayoutParams.WRAP_CONTENT);
    spinnerParams.setMargins(20, 20, 20, 20);
    spinnerParams.addRule(RelativeLayout.CENTER_HORIZONTAL, 1);
    spinnerParams.addRule(RelativeLayout.BELOW, BUTTON_ID);

    layout.addView(progressBar, spinnerParams);

    setContentView(layout);

    mTracingButton = traceButton;
    mProgressBar = progressBar;
  }

  private TraceOrchestrator.TraceProvider[] calculateProviders() {
    String[] providers = BuildConfig.PROVIDERS.split("-");
    TraceOrchestrator.TraceProvider[] result =
        new TraceOrchestrator.TraceProvider[providers.length];
    int idx = 0;
    for (String provider : providers) {
      switch (provider) {
        case "atrace":
          result[idx++] = new SystraceProvider();
          break;
        case "systemcounters":
          result[idx++] = new SystemCounterThread();
          break;
        case "stacktrace":
          result[idx++] = new StackFrameThread(getApplicationContext());
          break;
        case "threadmetadata":
          result[idx++] = new ThreadMetadataProvider();
          break;
        case "yarn":
          result[idx++] = new PerfEventsProvider();
          break;
        default:
          throw new RuntimeException("Could not map provider " + provider);
      }
    }
    return result;
  }
}
