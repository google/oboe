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
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Menu
import com.google.oboe.samples.powerplay.effects.EffectsController

@Composable
fun EffectsBottomSheet(
    effectsController: EffectsController?,
    isOffloadMode: Boolean = false,
    onDismiss: () -> Unit
) {
    if (effectsController == null) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(stringResource(id = R.string.effects_not_initialized))
        }
        return
    }

    val supportedEffects = remember { effectsController.getSupportedEffects() }
    
    if (supportedEffects.isEmpty()) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(stringResource(id = R.string.no_supported_effects))
        }
        return
    }

    var selectedTab by remember { mutableIntStateOf(0) }
    val tabs = supportedEffects.map { effect ->
        when (effect) {
            EffectsController.EffectType.EQUALIZER -> R.string.title_equalizer
            EffectsController.EffectType.BASS_BOOST -> R.string.title_bass_boost
            EffectsController.EffectType.LOUDNESS -> R.string.title_loudness
        }
    }
    val icons = supportedEffects.map { effect ->
        when (effect) {
            EffectsController.EffectType.EQUALIZER -> Icons.Default.Menu
            EffectsController.EffectType.BASS_BOOST -> Icons.Default.ThumbUp
            EffectsController.EffectType.LOUDNESS -> Icons.Default.Star
        }
    }
    val descriptions = supportedEffects.map { effect ->
        when (effect) {
            EffectsController.EffectType.EQUALIZER -> R.string.desc_equalizer
            EffectsController.EffectType.BASS_BOOST -> R.string.desc_bass_boost
            EffectsController.EffectType.LOUDNESS -> R.string.desc_loudness
        }
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 32.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        if (isOffloadMode) {
            Text(
                text = stringResource(id = R.string.offload_mode_warning),
                color = Color.Red,
                fontSize = 12.sp,
                modifier = Modifier.padding(8.dp)
            )
        }
        TabRow(selectedTabIndex = selectedTab) {
            tabs.forEachIndexed { index, titleResId ->
                val title = stringResource(id = titleResId)
                Tab(
                    selected = selectedTab == index,
                    onClick = { selectedTab = index },
                    icon = { androidx.compose.material3.Icon(imageVector = icons[index], contentDescription = title) }
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text(
            text = stringResource(id = tabs[selectedTab]),
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
            val currentEffect = supportedEffects[selectedTab]
            when (currentEffect) {
                EffectsController.EffectType.EQUALIZER -> EqualizerTab(effectsController.equalizer)
                EffectsController.EffectType.BASS_BOOST -> BassBoostTab(effectsController.bassBoost)
                EffectsController.EffectType.LOUDNESS -> LoudnessTab(effectsController.loudness)
            }
        }
    }
}
