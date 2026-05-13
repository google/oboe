package com.google.oboe.samples.powerplay.ui.effects

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Checkbox
import androidx.compose.material3.MaterialTheme
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
import androidx.compose.ui.unit.dp
import com.google.oboe.samples.powerplay.effects.LoudnessManager

@Composable
fun LoudnessTab(loudnessManager: LoudnessManager?) {
    if (loudnessManager == null) {
        Text("Loudness Enhancer not available")
        return
    }

    var targetGain by remember { mutableFloatStateOf(loudnessManager.getTargetGain().toFloat()) }

    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "Target Gain: ${"%.1f".format(targetGain / 100f)} dB",
            style = MaterialTheme.typography.bodyMedium
        )
        Slider(
            value = targetGain,
            onValueChange = {
                targetGain = it
                loudnessManager.setTargetGain(it.toInt())
            },
            valueRange = 0f..2000f, // 0 to 20 dB
            modifier = Modifier.fillMaxWidth()
        )
    }
}
