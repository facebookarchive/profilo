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

import android.app.Activity;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.ToggleButton;
import com.facebook.profilo.BuildConfig;
import com.facebook.profilo.controllers.external.ExternalController;
import com.facebook.profilo.controllers.external.api.ExternalTraceControl;
import com.facebook.profilo.core.ProfiloConstants;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.core.TraceController;
import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.soloader.SoLoader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public abstract class BaseSampleAppMainActivity extends Activity {

  private WorkloadThread mWorkerThread;
  private ToggleButton mTracingButton;
  private ProgressBar mProgressBar;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    initializeProfilo();

    createLayout();

    mWorkerThread = new WorkloadThread("busy-thread");
    mWorkerThread.start();
    mTracingButton.setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            if (mTracingButton.isChecked()) {
              ExternalTraceControl.startTrace(
                  getProvidersToEnable(),
                  /*cpuSamplingRateMs*/ 10);
              mProgressBar.setVisibility(View.VISIBLE);
            } else {
              ExternalTraceControl.stopTrace();
              mProgressBar.setVisibility(View.INVISIBLE);
            }
          }
        });
  }

  private void initializeProfilo() {
    try {
      SoLoader.init(this.getApplicationContext(), 0);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }

    SparseArray<TraceController> controllers = new SparseArray<>(1);
    controllers.put(ProfiloConstants.TRIGGER_EXTERNAL, new ExternalController());

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
      TraceOrchestrator.TraceProvider[] result =
          new TraceOrchestrator.TraceProvider[BuildConfig.PROVIDERS.length];
      int idx = 0;
      for (String provider : BuildConfig.PROVIDERS) {
        result[idx++] = createProvider(provider);
      }
      return result;
    }

  protected int getProvidersToEnable() {
    List<String> allProviders = ProvidersRegistry.getRegisteredProviders();
    ArrayList<String> validProviders = new ArrayList<>();
    for (String prov : allProviders) {
      if (prov.equals("wall_time_stack_trace") || prov.equals("native_stack_trace")) {
        continue;
      }
      validProviders.add(prov);
    }
    return ProvidersRegistry.getBitMaskFor(validProviders);
  }

  protected TraceOrchestrator.TraceProvider createProvider(String providerName) {
    throw new RuntimeException("Could not map provider " + providerName);
  }
}
