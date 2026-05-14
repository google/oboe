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
package com.google.oboe.samples.powerplay.ui.effects

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.google.oboe.samples.powerplay.effects.EqualizerManager

@Composable
fun EqualizerTab(equalizerManager: EqualizerManager?) {
    if (equalizerManager == null) {
        Text("Equalizer not available")
        return
    }

    val equalizerBands = remember { equalizerManager.getBands() }
    var resetTrigger by remember { mutableStateOf(0) }

    Column(
        modifier = Modifier.fillMaxWidth().verticalScroll(rememberScrollState()),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        equalizerBands.forEach { band ->
            var level by remember(band.id, resetTrigger) { mutableFloatStateOf(band.currentLevelmB.toFloat()) }

            Spacer(modifier = Modifier.height(8.dp))
            Text(
                text = "${band.centerFreqHz} Hz: ${"%.1f".format(level / 100f)} dB",
                style = MaterialTheme.typography.bodyMedium
            )
            Slider(
                value = level,
                onValueChange = {
                    level = it
                    equalizerManager.setBandLevel(band.id, it.toInt().toShort())
                },
                valueRange = band.minLevelmB.toFloat()..band.maxLevelmB.toFloat(),
                modifier = Modifier.fillMaxWidth()
            )
        }
        Spacer(modifier = Modifier.height(16.dp))
        TextButton(
            onClick = {
                equalizerManager.reset()
                resetTrigger++
            }
        ) {
            Text("Reset to Defaults")
        }
    }
}
