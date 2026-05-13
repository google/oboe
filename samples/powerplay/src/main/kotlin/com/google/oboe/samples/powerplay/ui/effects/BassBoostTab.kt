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
import com.google.oboe.samples.powerplay.effects.BassBoostManager

@Composable
fun BassBoostTab(bassBoostManager: BassBoostManager?) {
    if (bassBoostManager == null) {
        Text("Bass Boost not available")
        return
    }

    var strength by remember { mutableFloatStateOf(bassBoostManager.getStrength().toFloat()) }

    Column(
        modifier = Modifier.fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(
            text = "Strength: ${strength.toInt()}",
            style = MaterialTheme.typography.bodyMedium
        )
        Slider(
            value = strength,
            onValueChange = {
                strength = it
                bassBoostManager.setStrength(it.toInt().toShort())
            },
            valueRange = 0f..1000f, // BassBoost strength is usually 0 to 1000
            modifier = Modifier.fillMaxWidth()
        )
    }
}
