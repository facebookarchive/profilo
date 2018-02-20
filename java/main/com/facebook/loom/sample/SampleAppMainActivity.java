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
import com.facebook.loom.controllers.external.ExternalController;
import com.facebook.loom.controllers.external.api.ExternalTraceControl;
import com.facebook.loom.core.LoomConstants;
import com.facebook.loom.core.TraceController;
import com.facebook.loom.core.TraceOrchestrator;
import com.facebook.loom.provider.atrace.SystraceProvider;
import com.facebook.loom.provider.processmetadata.ProcessMetadataProvider;
import com.facebook.loom.provider.stacktrace.StackFrameThread;
import com.facebook.loom.provider.systemcounters.SystemCounterThread;
import com.facebook.loom.provider.threadmetadata.ThreadMetadataProvider;
import com.facebook.loom.provider.yarn.PerfEventsProvider;
import com.facebook.soloader.SoLoader;
import java.io.IOException;

public class SampleAppMainActivity extends Activity {

  private WorkloadThread mWorkerThread;
  private ToggleButton mTracingButton;
  private ProgressBar mProgressBar;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    initializeLoom();

    createLayout();

    mWorkerThread = new WorkloadThread("busy-thread");
    mWorkerThread.start();
    mTracingButton.setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            if (mTracingButton.isChecked()) {
              ExternalTraceControl.startTrace(
                  LoomConstants.PROVIDER_STACK_FRAME
                      | LoomConstants.PROVIDER_SYSTEM_COUNTERS
                      | LoomConstants.PROVIDER_OTHER,
                  /*cpuSamplingRateMs*/ 10);
              mProgressBar.setVisibility(View.VISIBLE);
            } else {
              ExternalTraceControl.stopTrace();
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
    controllers.put(LoomConstants.TRIGGER_EXTERNAL, new ExternalController());

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
        case "processmetadata":
          result[idx++] = new ProcessMetadataProvider(this.getApplicationContext());
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
