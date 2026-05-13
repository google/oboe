package com.google.oboe.samples.powerplay.ui.effects

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.selection.selectable
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import com.google.oboe.samples.powerplay.effects.ReverbManager

@Composable
fun ReverbTab(reverbManager: ReverbManager?) {
    if (reverbManager == null) {
        Text("Reverb not available")
        return
    }

    var selectedPreset by remember { mutableStateOf(reverbManager.getPreset()) }
    var volume by remember { mutableFloatStateOf(reverbManager.getVolume().toFloat()) }

    val presetNames = mapOf(
        ReverbManager.Preset.NONE to "None",
        ReverbManager.Preset.SMALL_ROOM to "Small Room",
        ReverbManager.Preset.MEDIUM_ROOM to "Medium Room",
        ReverbManager.Preset.LARGE_HALL to "Large Hall",
        ReverbManager.Preset.PLATE to "Plate"
    )

    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(text = "Presets", style = MaterialTheme.typography.titleMedium)
        Spacer(modifier = Modifier.height(8.dp))

        Column(modifier = Modifier.fillMaxWidth()) {
            ReverbManager.Preset.values().forEach { preset ->
                Row(
                    Modifier
                        .fillMaxWidth()
                        .height(40.dp)
                        .selectable(
                            selected = (preset == selectedPreset),
                            onClick = {
                                selectedPreset = preset
                                reverbManager.setPreset(preset)
                            },
                            role = Role.RadioButton
                        )
                        .padding(horizontal = 8.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = (preset == selectedPreset),
                        onClick = null
                    )
                    Text(
                        text = presetNames[preset] ?: preset.name,
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier.padding(start = 12.dp)
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        val percentage = ((volume + 9000) / 110).toInt().coerceIn(0, 100)
        Text(
            text = "Reverb Volume: $percentage%",
            style = MaterialTheme.typography.bodyMedium
        )
        Spacer(modifier = Modifier.height(8.dp))
        Slider(
            value = volume,
            onValueChange = {
                volume = it
                reverbManager.setVolume(it.toInt().toShort())
            },
            valueRange = -9000f..2000f,
            modifier = Modifier.fillMaxWidth()
        )
    }
}
