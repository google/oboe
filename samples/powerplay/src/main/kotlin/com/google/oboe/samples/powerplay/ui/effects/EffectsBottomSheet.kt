package com.google.oboe.samples.powerplay.ui.effects

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.res.stringResource
import com.google.oboe.samples.powerplay.R
import androidx.compose.animation.animateContentSize
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.ThumbUp
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Menu
import com.google.oboe.samples.powerplay.effects.EffectsController

@Composable
fun EffectsBottomSheet(
    effectsController: EffectsController?,
    onDismiss: () -> Unit
) {
    if (effectsController == null) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("Effects not initialized. Ensure stream is open with session ID.")
        }
        return
    }

    var selectedTab by remember { mutableIntStateOf(0) }
    val tabs = listOf("Equalizer", "Bass Boost", "Virtualizer", "Reverb", "Loudness")
    val icons = listOf(
        Icons.Default.Menu,
        Icons.Default.ThumbUp,
        Icons.Default.Refresh,
        Icons.Default.Notifications,
        Icons.Default.Star
    )
    val descriptions = listOf(
        R.string.desc_equalizer,
        R.string.desc_bass_boost,
        R.string.desc_virtualizer,
        R.string.desc_reverb,
        R.string.desc_loudness
    )

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 32.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        TabRow(selectedTabIndex = selectedTab) {
            tabs.forEachIndexed { index, title ->
                Tab(
                    selected = selectedTab == index,
                    onClick = { selectedTab = index },
                    icon = { androidx.compose.material3.Icon(imageVector = icons[index], contentDescription = title) }
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = tabs[selectedTab],
            fontSize = 20.sp,
            style = MaterialTheme.typography.titleLarge,
            color = Color.Black,
            modifier = Modifier
                .padding(horizontal = 24.dp)
                .padding(bottom = 4.dp)
        )

        Text(
            text = stringResource(id = descriptions[selectedTab]),
            fontSize = 14.sp,
            style = MaterialTheme.typography.bodyMedium,
            color = Color.Gray,
            modifier = Modifier
                .padding(horizontal = 24.dp)
                .padding(bottom = 16.dp)
        )

        Spacer(modifier = Modifier.height(16.dp))

        androidx.compose.foundation.layout.Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .animateContentSize()
        ) {
            when (selectedTab) {
                0 -> EqualizerTab(effectsController.equalizer)
                1 -> BassBoostTab(effectsController.bassBoost)
                2 -> VirtualizerTab(effectsController.virtualizer)
                3 -> ReverbTab(effectsController.reverb)
                4 -> LoudnessTab(effectsController.loudness)
            }
        }
    }
}
