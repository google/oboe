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
import androidx.compose.material.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.lifecycle.ViewModel
import com.example.minimaloboe.ui.theme.SamplesTheme
import kotlinx.coroutines.*

class MainActivity : ComponentActivity() {

    var mExampleViewModel = ExampleViewModel()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SamplesTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colors.background
                ) {
                    MainControls(mExampleViewModel)
                }
            }
        }
    }

    override fun onStop() {
        // call the superclass method first
        super.onStop()
        mExampleViewModel.stopAudio()
    }
}

class ExampleViewModel : ViewModel() {

    var audioPlayer = AudioPlayer()

    fun startAudio(): Int {
        return audioPlayer.startAudio()
    }

    fun stopAudio(): Int {
        return audioPlayer.stopAudio()
    }
}

@Composable
fun MainControls(viewModel: ExampleViewModel = ExampleViewModel()) {

    val NO_RESULT_YET = 1

    // State that affects the UI.
    var started by remember { mutableStateOf(false) }
    var result by remember { mutableStateOf(NO_RESULT_YET) }

    Column {
        Text(text = "Minimal Oboe!")
        // START button
        Button(
            onClick = {
                // Start Oboe from a coroutine in case it blocks for too long.
                // If the AudioServer has died it may take several seconds to recover.
                // That can cause an ANR if we are starting audio from the main UI thread.
                GlobalScope.launch {
                    result = viewModel.startAudio()
                    started = (result == 0)
                }
            },
            enabled = !started
        ) {
            Text(text = "Start Audio")
        }
        // STOP button
        Button(
            onClick = {
                GlobalScope.launch {
                    result = viewModel.stopAudio()
                    started = false
                }
            },
            enabled = started
        ) {
            Text(text = "Stop Audio")
        }
        Text("Result = " + (if (result == NO_RESULT_YET) "?" else result))
    }
}

@Preview(showBackground = true)
@Composable
fun DefaultPreview() {
    SamplesTheme {
        MainControls()
    }
}
