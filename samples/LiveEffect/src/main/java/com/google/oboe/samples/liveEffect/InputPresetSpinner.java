package com.google.oboe.samples.liveEffect;
/*
  Copyright 2020 The Android Open Source Project

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;

public class InputPresetSpinner extends Spinner {

    public InputPresetSpinner(Context context){
        super(context);
        setup(context);
    }

    public InputPresetSpinner(Context context, int mode){
        super(context, mode);
        setup(context);
    }

    public InputPresetSpinner(Context context, AttributeSet attrs){
        super(context, attrs);
        setup(context);
    }

    public InputPresetSpinner(Context context, AttributeSet attrs, int defStyleAttr){
        super(context, attrs, defStyleAttr);
        setup(context);
    }

    public InputPresetSpinner(Context context, AttributeSet attrs, int defStyleAttr, int mode){
        super(context, attrs, defStyleAttr, mode);
        setup(context);
    }

    public InputPresetSpinner(Context context, AttributeSet attrs, int defStyleAttr,
                              int defStyleRes, int mode){
        super(context, attrs, defStyleAttr, defStyleRes, mode);
        setup(context);
    }
    public InputPresetSpinner(Context context, AttributeSet attrs, int defStyleAttr,
                              int defStyleRes, int mode, Resources.Theme popupTheme){
        super(context, attrs, defStyleAttr, defStyleRes, mode, popupTheme);
        setup(context);
    }
    
    private void setup(Context context) {

        ArrayAdapter<InputPreset> inputPresetArrayAdapter =
                new ArrayAdapter<InputPreset>(context, R.layout.audio_devices, getInputPresets()) {

                    @NonNull
                    @Override
                    public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
                        return getDropDownView(position, convertView, parent);
                    }

                    @Override
                    public View getDropDownView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
                        View rowView = convertView;
                        if (rowView == null) {
                            LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                            rowView = inflater.inflate(R.layout.spinner_item, parent, false);
                        }

                        TextView description = rowView.findViewById(R.id.spinner_item_description);
                        InputPreset inputPreset = (InputPreset) getItem(position);
                        description.setText(inputPreset.description);

                        return rowView;
                    }

                };
        setAdapter(inputPresetArrayAdapter);
        setSelection(5); // Default to VoiceRecognition.
    }

    ArrayList<InputPreset> getInputPresets() {
        ArrayList<InputPreset> inputPresets = new ArrayList<>();
        inputPresets.add(new InputPreset(5, "Camcorder"));
        inputPresets.add(new InputPreset(1, "Generic"));
        inputPresets.add(new InputPreset(9, "Unprocessed"));
        inputPresets.add(new InputPreset(7, "VoiceCommunication"));
        inputPresets.add(new InputPreset(10, "VoiceProcessing"));
        inputPresets.add(new InputPreset(6, "VoiceRecognition"));
        return inputPresets;
    }

    static final class InputPreset {
        public final int id;
        public final String description;

        public InputPreset(final int id, final String description) {
            this.id = id;
            this.description = description;
        }
    }
}
