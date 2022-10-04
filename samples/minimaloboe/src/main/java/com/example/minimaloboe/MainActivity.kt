/*
 * Copyright 2022 The Android Open Source Project
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

package com.example.minimaloboe

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.Button
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.ViewModel
import androidx.lifecycle.compose.ExperimentalLifecycleComposeApi
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.minimaloboe.ui.theme.SamplesTheme
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

class MainActivity : ComponentActivity() {

    @OptIn(ExperimentalLifecycleComposeApi::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SamplesTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colors.background
                ) {
                    MainControls()
                }
            }
        }
    }
}

class ExampleViewModel : ViewModel(), DefaultLifecycleObserver {

    private val _uiState = MutableStateFlow<PlayingUiState>(PlayingUiState.NoResultYet)
    val uiState : StateFlow<PlayingUiState> = _uiState.asStateFlow()

    private val audioPlayer = AudioPlayer()

    fun setPlaybackEnabled(isEnabled: Boolean){
        // Start (and stop) Oboe from a coroutine in case it blocks for too long.
        // If the AudioServer has died it may take several seconds to recover.
        // That can cause an ANR if we are starting audio from the main UI thread.
        viewModelScope.launch {
            val result = if (isEnabled){
                audioPlayer.startAudio()
            } else {
                audioPlayer.stopAudio()
            }

            val newUiState = if (result == 0){
                if (isEnabled){
                    PlayingUiState.Started
                } else {
                    PlayingUiState.Stopped
                }
            } else {
                PlayingUiState.Unknown(result)
            }

            _uiState.update { newUiState }
        }
    }

    override fun onStop(owner: LifecycleOwner) {
        super.onStop(owner)
        setPlaybackEnabled(false)
    }

}

sealed interface PlayingUiState {
    object NoResultYet : PlayingUiState
    object Started : PlayingUiState
    object Stopped : PlayingUiState
    data class Unknown(val resultCode: Int) : PlayingUiState
}

@ExperimentalLifecycleComposeApi
@Composable
fun MainControls(viewModel: ExampleViewModel = viewModel()) {
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()
    MainControls(uiState, viewModel::setPlaybackEnabled)
}

@ExperimentalLifecycleComposeApi
@Composable
fun MainControls(uiState: PlayingUiState, setPlaybackEnabled: (Boolean) -> Unit){

    Column {

        val isPlaying = uiState is PlayingUiState.Started

        Text(text = "Minimal Oboe!")

        // Display a button for starting and stopping playback.
        val buttonText = if (isPlaying){
            "Stop Audio"
        } else {
            "Start Audio"
        }

        Button(onClick = { setPlaybackEnabled(!isPlaying) }
        ) {
            Text(text = buttonText)
        }

        // Create a status message for displaying the current playback state.
        val uiStatusMessage = "Current status: " +
            when (uiState){
                PlayingUiState.NoResultYet -> "No result yet"
                PlayingUiState.Started -> "Started"
                PlayingUiState.Stopped -> "Stopped"
                is PlayingUiState.Unknown -> {
                    "Unknown. Result = " + (uiState as PlayingUiState.Unknown).resultCode
                }
            }

        Text(uiStatusMessage)
    }
}

@OptIn(ExperimentalLifecycleComposeApi::class)
@Preview(showBackground = true)
@Composable
fun DefaultPreview() {
    SamplesTheme {
        MainControls(PlayingUiState.Started) { }
    }
}
