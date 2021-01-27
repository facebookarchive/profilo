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
package com.facebook.profilo.sample;

import android.app.Activity;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.ToggleButton;
import com.facebook.profilo.BuildConfig;
import com.facebook.profilo.controllers.external.ExternalController;
import com.facebook.profilo.controllers.external.api.ExternalTraceControl;
import com.facebook.profilo.core.BaseTraceProvider;
import com.facebook.profilo.core.ProvidersRegistry;
import com.facebook.profilo.core.TraceController;
import com.facebook.profilo.core.TraceOrchestrator;
import com.facebook.profilo.logger.Trace;
import com.facebook.soloader.SoLoader;
import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

public abstract class BaseSampleAppMainActivity extends Activity {

  private static final int TRACING_BUTTON_ID = 0x100;
  protected static final int RELATIVE_LAYOUT_ID = 0x101;
  protected static final int TRACE_CONFIG_LAYOUT_ID = 0x102;
  private WorkloadThread mWorkerThread;
  private ToggleButton mTracingButton;
  private ProgressBar mProgressBar;
  private CheckBox mMemoryOnlyCheckbox;
  private HashMap<String, CheckBox> mProviderCheckboxes;

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
            boolean memoryOnly = mMemoryOnlyCheckbox.isChecked();
            if (mTracingButton.isChecked()) {
              int flags = Trace.FLAG_MANUAL | (memoryOnly ? Trace.FLAG_MEMORY_ONLY : 0);
              ExternalTraceControl.startTrace(
                  calculateEnabledProviders(),
                  10, // cpuSamplingRate
                  flags);
              mProgressBar.setVisibility(View.VISIBLE);
            } else {
              ExternalTraceControl.stopTrace();
              mProgressBar.setVisibility(View.INVISIBLE);
            }
          }
        });
  }

  protected void initializeProfilo() {
    try {
      SoLoader.init(this.getApplicationContext(), 0);
    } catch (IOException e) {
      throw new RuntimeException(e);
    }

    SparseArray<TraceController> controllers = new SparseArray<>(1);
    controllers.put(ExternalController.TRIGGER_EXTERNAL, new ExternalController());

    TraceOrchestrator.initialize(
        this.getApplicationContext(),
        null, /* ConfigProvider */
        TraceOrchestrator.MAIN_PROCESS_NAME,
        true, /* isMainProcess */
        calculateAvailableProviders(),
        controllers,
        null /* traceFolder */);
  }

  private void createLayout() {
    RelativeLayout layout = new RelativeLayout(this);
    ToggleButton traceButton = new ToggleButton(this);
    ProgressBar progressBar = new ProgressBar(this);
    layout.setId(RELATIVE_LAYOUT_ID);

    traceButton.setTextOff("Start tracing");
    traceButton.setTextOn("Stop tracing");
    traceButton.setChecked(false); // force the string to update
    traceButton.setId(TRACING_BUTTON_ID);

    progressBar.setIndeterminate(true);
    progressBar.setVisibility(View.INVISIBLE);

    RelativeLayout.LayoutParams buttonParams =
        new RelativeLayout.LayoutParams(
            ViewGroup.MarginLayoutParams.WRAP_CONTENT, ViewGroup.MarginLayoutParams.WRAP_CONTENT);
    buttonParams.setMargins(20, 20, 20, 20);
    buttonParams.addRule(RelativeLayout.ALIGN_PARENT_TOP, 1);
    buttonParams.addRule(RelativeLayout.CENTER_HORIZONTAL, 1);

    layout.addView(traceButton, buttonParams);

    RelativeLayout.LayoutParams spinnerParams =
        new RelativeLayout.LayoutParams(
            ViewGroup.MarginLayoutParams.WRAP_CONTENT, ViewGroup.MarginLayoutParams.WRAP_CONTENT);
    spinnerParams.setMargins(20, 20, 20, 20);
    spinnerParams.addRule(RelativeLayout.ALIGN_PARENT_TOP, 1);
    spinnerParams.addRule(RelativeLayout.RIGHT_OF, TRACING_BUTTON_ID);

    layout.addView(progressBar, spinnerParams);

    LinearLayout traceConfigLayout = new LinearLayout(this);
    traceConfigLayout.setId(TRACE_CONFIG_LAYOUT_ID);
    traceConfigLayout.setOrientation(LinearLayout.VERTICAL);

    ScrollView.LayoutParams traceConfigParams =
        new ScrollView.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

    RelativeLayout.LayoutParams scrollViewParams =
        new RelativeLayout.LayoutParams(
            ViewGroup.MarginLayoutParams.MATCH_PARENT, ViewGroup.MarginLayoutParams.WRAP_CONTENT);
    scrollViewParams.setMargins(20, 20, 20, 20);
    scrollViewParams.addRule(RelativeLayout.CENTER_HORIZONTAL, 1);
    scrollViewParams.addRule(RelativeLayout.BELOW, TRACING_BUTTON_ID);

    CheckBox memoryOnlyCheckbox = new CheckBox(this);
    memoryOnlyCheckbox.setText("In-memory trace");

    traceConfigLayout.addView(
        memoryOnlyCheckbox,
        LinearLayout.LayoutParams.WRAP_CONTENT,
        LinearLayout.LayoutParams.WRAP_CONTENT);
    HashMap<String, CheckBox> providerViews = createProviderViews(traceConfigLayout);

    ScrollView scrollView = new ScrollView(this);
    scrollView.addView(traceConfigLayout, traceConfigParams);

    layout.addView(scrollView, scrollViewParams);

    setContentView(layout);

    mTracingButton = traceButton;
    mProgressBar = progressBar;
    mMemoryOnlyCheckbox = memoryOnlyCheckbox;
    mProviderCheckboxes = providerViews;
  }

  private HashMap<String, CheckBox> createProviderViews(LinearLayout layout) {
    List<String> allProviders = ProvidersRegistry.getRegisteredProviders();
    Collections.sort(allProviders);

    HashMap<String, CheckBox> checkboxes = new HashMap<>();
    for (String provider : allProviders) {
      CheckBox box = new CheckBox(layout.getContext());
      box.setText(provider);
      box.setChecked(true);

      layout.addView(box);
      checkboxes.put(provider, box);
    }
    return checkboxes;
  }

  private int calculateEnabledProviders() {
    int mask = 0;
    for (HashMap.Entry<String, CheckBox> provider : mProviderCheckboxes.entrySet()) {
      if (provider.getValue().isChecked()) {
        mask |= ProvidersRegistry.getBitMaskFor(provider.getKey());
      }
    }
    return mask;
  }

  protected BaseTraceProvider[] calculateAvailableProviders() {
    BaseTraceProvider[] result = new BaseTraceProvider[BuildConfig.PROVIDERS.length];
    int idx = 0;
    for (String provider : BuildConfig.PROVIDERS) {
      result[idx++] = createProvider(provider);
    }
    return result;
  }

  protected BaseTraceProvider createProvider(String providerName) {
    throw new RuntimeException("Could not map provider " + providerName);
  }
}
