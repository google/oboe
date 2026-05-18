/*
 * Copyright 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.mobileer.oboetester;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.os.Bundle;
import android.os.Parcelable;
import java.util.ArrayList;
import java.util.List;

public class FrequencySettingView extends LinearLayout {

    public static int getSignalIndexForSource(int sourceResId) {
        if (sourceResId == R.string.source_white_noise) {
            return 0;
        } else if (sourceResId == R.string.source_sine) {
            return 1;
        } else if (sourceResId == R.string.source_silence) {
            return 2;
        }
        return 0;
    }

    private Context mContext;
    private RadioGroup mRadioGroupBands;
    private LinearLayout mBandSpecContainer;
    private Spinner mPresetSpinner;
    private List<EditText> mFrequencyAnchorInputs = new ArrayList<>();
    private List<BandInputs> mBandInputsList = new ArrayList<>();

    private boolean mIsLoadingPreset = false;
    private OnSettingChangedListener mListener;

    public interface OnSettingChangedListener {

        void onSettingChanged();
    }

    private static class BandInputs {

        EditText startTop;
        EditText stopTop;
        EditText startBottom;
        EditText stopBottom;
    }

    private List<FrequencyPreset> mPresets = new ArrayList<>();

    public FrequencySettingView(Context context) {
        super(context);
        initializeViews(context, null);
    }

    public FrequencySettingView(Context context, AttributeSet attrs) {
        super(context, attrs);
        initializeViews(context, attrs);
    }

    public FrequencySettingView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        initializeViews(context, attrs);
    }

    private void initializeViews(Context context, AttributeSet attrs) {
        LayoutInflater inflater = (LayoutInflater) context
                .getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        inflater.inflate(R.layout.frequency_setting, this);
        setOrientation(LinearLayout.VERTICAL);

        mContext = context;
        mRadioGroupBands = findViewById(R.id.radioGroupBands);
        mBandSpecContainer = findViewById(R.id.bandSpecContainer);
        mPresetSpinner = findViewById(R.id.spinnerPresets);
    }

    public void initialize(int group, OnSettingChangedListener listener) {
        mListener = listener;

        FrequencyPresetRepository repo = new FrequencyPresetRepository(group);
        mPresets = repo.getPresets();
        setupPresetSpinner();

        mRadioGroupBands.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                if (!mIsLoadingPreset) {
                    notifySettingChanged();
                }
                int numBands = (checkedId == R.id.radio4Bands) ? 4 : 3;
                rebuildBandSpecUi(numBands);
            }
        });

        rebuildBandSpecUi(3); // Default to 3 bands
    }

    private void setupPresetSpinner() {
        List<String> presetNames = new ArrayList<>();
        presetNames.add("Custom");
        for (FrequencyPreset p : mPresets) {
            presetNames.add(p.name);
        }

        android.widget.ArrayAdapter<String> adapter = new android.widget.ArrayAdapter<>(mContext,
                android.R.layout.simple_spinner_item, presetNames);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        mPresetSpinner.setAdapter(adapter);

        mPresetSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                if (position > 0) {
                    loadPreset(mPresets.get(position - 1));
                }
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {
            }
        });

        mPresetSpinner.setSelection(1); // Default to Loopback Dongle
    }

    private void loadPreset(FrequencyPreset preset) {
        mIsLoadingPreset = true;

        // Update RadioGroup
        int numBands = preset.bands.size();
        mRadioGroupBands.check(numBands == 4 ? R.id.radio4Bands : R.id.radio3Bands);
        rebuildBandSpecUi(numBands);

        // Update Anchors
        for (int i = 0; i < preset.anchors.length && i < mFrequencyAnchorInputs.size(); i++) {
            mFrequencyAnchorInputs.get(i).setText(String.valueOf(preset.anchors[i]));
        }

        // Update Thresholds
        for (int i = 0; i < preset.bands.size() && i < mBandInputsList.size(); i++) {
            BandInputs inputs = mBandInputsList.get(i);
            FrequencyBandSpec.BandThreshold band = preset.bands.get(i);
            inputs.startTop.setText(String.valueOf(band.startTop));
            inputs.stopTop.setText(String.valueOf(band.stopTop));
            inputs.startBottom.setText(String.valueOf(band.startBottom));
            inputs.stopBottom.setText(String.valueOf(band.stopBottom));
        }

        mIsLoadingPreset = false;
        if (mListener != null) {
            mListener.onSettingChanged(); // Notify of preset change for input preset configuration
        }
    }

    private void notifySettingChanged() {
        if (mIsLoadingPreset) {
            return;
        }
        mPresetSpinner.setSelection(0, false); // Set to Custom
        if (mListener != null) {
            mListener.onSettingChanged();
        }
    }

    private TextWatcher mTextWatcher = new TextWatcher() {
        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
        }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
        }

        @Override
        public void afterTextChanged(Editable s) {
            notifySettingChanged();
        }
    };

    private void rebuildBandSpecUi(int numBands) {
        mBandSpecContainer.removeAllViews();
        mFrequencyAnchorInputs.clear();
        mBandInputsList.clear();

        TextView anchorsTitle = new TextView(mContext);
        anchorsTitle.setText("Frequency Anchors (Hz):");
        anchorsTitle.setTypeface(null, android.graphics.Typeface.BOLD);
        mBandSpecContainer.addView(anchorsTitle);

        LinearLayout anchorsLayout = new LinearLayout(mContext);
        anchorsLayout.setOrientation(LinearLayout.HORIZONTAL);

        int numAnchors = numBands + 1;

        for (int i = 0; i < numAnchors; i++) {
            EditText input = new EditText(mContext);
            input.setInputType(android.text.InputType.TYPE_CLASS_NUMBER);
            input.setHint("F" + i);
            input.addTextChangedListener(mTextWatcher);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);
            input.setLayoutParams(params);
            anchorsLayout.addView(input);
            mFrequencyAnchorInputs.add(input);
        }
        mBandSpecContainer.addView(anchorsLayout);

        for (int b = 0; b < numBands; b++) {
            TextView bandTitle = new TextView(mContext);
            bandTitle.setText("Band " + (b + 1) + " Thresholds (dBFS):");
            bandTitle.setTypeface(null, android.graphics.Typeface.BOLD);
            bandTitle.setPadding(0, 10, 0, 0);
            mBandSpecContainer.addView(bandTitle);

            BandInputs bandInputs = new BandInputs();

            LinearLayout topRow = new LinearLayout(mContext);
            topRow.setOrientation(LinearLayout.HORIZONTAL);
            TextView topLabel = new TextView(mContext);
            topRow.addView(topLabel);
            LinearLayout.LayoutParams labelParams = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            topLabel.setText("Top: ");
            topLabel.setLayoutParams(labelParams);

            LinearLayout.LayoutParams inputParams = new LinearLayout.LayoutParams(
                    0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);

            bandInputs.startTop = new EditText(mContext);
            bandInputs.startTop.setInputType(android.text.InputType.TYPE_CLASS_NUMBER
                    | android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);
            bandInputs.startTop.setLayoutParams(inputParams);
            bandInputs.startTop.setHint("Start");
            bandInputs.startTop.addTextChangedListener(mTextWatcher);
            topRow.addView(bandInputs.startTop);

            bandInputs.stopTop = new EditText(mContext);
            bandInputs.stopTop.setInputType(android.text.InputType.TYPE_CLASS_NUMBER
                    | android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);
            bandInputs.stopTop.setLayoutParams(inputParams);
            bandInputs.stopTop.setHint("Stop");
            bandInputs.stopTop.addTextChangedListener(mTextWatcher);
            topRow.addView(bandInputs.stopTop);

            mBandSpecContainer.addView(topRow);

            LinearLayout bottomRow = new LinearLayout(mContext);
            bottomRow.setOrientation(LinearLayout.HORIZONTAL);
            TextView bottomLabel = new TextView(mContext);
            bottomRow.addView(bottomLabel);
            bottomLabel.setText("Bot: ");
            bottomLabel.setLayoutParams(labelParams);

            bandInputs.startBottom = new EditText(mContext);
            bandInputs.startBottom.setInputType(android.text.InputType.TYPE_CLASS_NUMBER
                    | android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);
            bandInputs.startBottom.setLayoutParams(inputParams);
            bandInputs.startBottom.setHint("Start");
            bandInputs.startBottom.addTextChangedListener(mTextWatcher);
            bottomRow.addView(bandInputs.startBottom);

            bandInputs.stopBottom = new EditText(mContext);
            bandInputs.stopBottom.setInputType(android.text.InputType.TYPE_CLASS_NUMBER
                    | android.text.InputType.TYPE_NUMBER_FLAG_SIGNED);
            bandInputs.stopBottom.setLayoutParams(inputParams);
            bandInputs.stopBottom.setHint("Stop");
            bandInputs.stopBottom.addTextChangedListener(mTextWatcher);
            bottomRow.addView(bandInputs.stopBottom);

            mBandSpecContainer.addView(bottomRow);
            mBandInputsList.add(bandInputs);
        }
    }

    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mRadioGroupBands.setEnabled(enabled);
        for (int i = 0; i < mRadioGroupBands.getChildCount(); i++) {
            mRadioGroupBands.getChildAt(i).setEnabled(enabled);
        }
        mPresetSpinner.setEnabled(enabled);
        setEnabledRecursive(mBandSpecContainer, enabled);
    }

    private void setEnabledRecursive(ViewGroup viewGroup, boolean enabled) {
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View child = viewGroup.getChildAt(i);
            child.setEnabled(enabled);
            if (child instanceof ViewGroup) {
                setEnabledRecursive((ViewGroup) child, enabled);
            }
        }
    }

    public FrequencyPreset getActivePreset() {
        int pos = mPresetSpinner.getSelectedItemPosition();
        if (pos > 0) {
            return mPresets.get(pos - 1);
        }
        return null;
    }

    public FrequencyBandSpec getSpec() {
        int numAnchors = mFrequencyAnchorInputs.size();
        int[] anchors = new int[numAnchors];
        for (int i = 0; i < numAnchors; i++) {
            try {
                anchors[i] = Integer.parseInt(mFrequencyAnchorInputs.get(i).getText().toString());
            } catch (NumberFormatException e) {
                anchors[i] = 0;
            }
        }

        List<FrequencyBandSpec.BandThreshold> bands = new ArrayList<>();
        for (BandInputs inputs : mBandInputsList) {
            float startTop = 0, stopTop = 0, startBottom = 0, stopBottom = 0;
            try {
                startTop = Float.parseFloat(inputs.startTop.getText().toString());
            } catch (NumberFormatException e) {
            }
            try {
                stopTop = Float.parseFloat(inputs.stopTop.getText().toString());
            } catch (NumberFormatException e) {
            }
            try {
                startBottom = Float.parseFloat(inputs.startBottom.getText().toString());
            } catch (NumberFormatException e) {
            }
            try {
                stopBottom = Float.parseFloat(inputs.stopBottom.getText().toString());
            } catch (NumberFormatException e) {
            }
            bands.add(new FrequencyBandSpec.BandThreshold(startTop, stopTop, startBottom,
                    stopBottom));
        }

        FrequencyPreset active = getActivePreset();
        FrequencyPreset.Band1CheckType checkType = FrequencyPreset.Band1CheckType.NONE;
        float threshold = 0.0f;
        if (active != null) {
            checkType = active.band1CheckType;
            threshold = active.band1Threshold;
        }
        return new FrequencyBandSpec(anchors, bands, checkType, threshold);
    }

    @Override
    protected Parcelable onSaveInstanceState() {
        Bundle bundle = new Bundle();
        bundle.putParcelable("superState", super.onSaveInstanceState());

        String[] anchorTexts = new String[mFrequencyAnchorInputs.size()];
        for (int i = 0; i < mFrequencyAnchorInputs.size(); i++) {
            anchorTexts[i] = mFrequencyAnchorInputs.get(i).getText().toString();
        }
        bundle.putStringArray("anchors", anchorTexts);

        String[] startTopTexts = new String[mBandInputsList.size()];
        String[] stopTopTexts = new String[mBandInputsList.size()];
        String[] startBotTexts = new String[mBandInputsList.size()];
        String[] stopBotTexts = new String[mBandInputsList.size()];
        for (int i = 0; i < mBandInputsList.size(); i++) {
            BandInputs bi = mBandInputsList.get(i);
            startTopTexts[i] = bi.startTop.getText().toString();
            stopTopTexts[i] = bi.stopTop.getText().toString();
            startBotTexts[i] = bi.startBottom.getText().toString();
            stopBotTexts[i] = bi.stopBottom.getText().toString();
        }
        bundle.putStringArray("startTop", startTopTexts);
        bundle.putStringArray("stopTop", stopTopTexts);
        bundle.putStringArray("startBot", startBotTexts);
        bundle.putStringArray("stopBot", stopBotTexts);

        return bundle;
    }

    @Override
    protected void onRestoreInstanceState(Parcelable state) {
        if (state instanceof Bundle) {
            Bundle bundle = (Bundle) state;
            super.onRestoreInstanceState(bundle.getParcelable("superState"));

            String[] anchorTexts = bundle.getStringArray("anchors");
            if (anchorTexts != null) {
                for (int i = 0; i < anchorTexts.length && i < mFrequencyAnchorInputs.size(); i++) {
                    mFrequencyAnchorInputs.get(i).setText(anchorTexts[i]);
                }
            }

            String[] startTopTexts = bundle.getStringArray("startTop");
            String[] stopTopTexts = bundle.getStringArray("stopTop");
            String[] startBotTexts = bundle.getStringArray("startBot");
            String[] stopBotTexts = bundle.getStringArray("stopBot");
            if (startTopTexts != null && stopTopTexts != null && startBotTexts != null
                    && stopBotTexts != null) {
                for (int i = 0; i < mBandInputsList.size() && i < startTopTexts.length; i++) {
                    BandInputs bi = mBandInputsList.get(i);
                    bi.startTop.setText(startTopTexts[i]);
                    bi.stopTop.setText(stopTopTexts[i]);
                    bi.startBottom.setText(startBotTexts[i]);
                    bi.stopBottom.setText(stopBotTexts[i]);
                }
            }
        } else {
            super.onRestoreInstanceState(state);
        }
    }
}
