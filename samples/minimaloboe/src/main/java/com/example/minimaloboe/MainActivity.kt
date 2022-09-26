package com.example.minimaloboe

import android.content.Context
import android.content.ContextWrapper
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material.Button
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import com.example.minimaloboe.ui.theme.SamplesTheme

class MainActivity : ComponentActivity() {

    var mPlayer = AudioPlayer()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            SamplesTheme {
                // A surface container using the 'background' color from the theme
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colors.background
                ) {
                    MainControls(mPlayer)
                }
            }
        }
    }
}

@Composable
fun StartButton(audioPlayer: AudioPlayer) {
    Button(onClick = {
        audioPlayer.startAudio()
    }) {
        Text(text = "Start")
    }
}

@Composable
fun StopButton(audioPlayer: AudioPlayer) {
    Button(onClick = {
        audioPlayer.stopAudio()
    }) {
        Text(text = "Stop")
    }
}

@Composable
fun MainControls(audioPlayer: AudioPlayer) {
    Column {
        Text(text = "Minimal Oboe!")
        StartButton(audioPlayer)
        StopButton(audioPlayer)
    }
}

fun Context.findActivity(): AppCompatActivity? = when (this) {
    is AppCompatActivity -> this
    is ContextWrapper -> baseContext.findActivity()
    else -> null
}

@Preview(showBackground = true)
@Composable
fun DefaultPreview() {
    var player = AudioPlayer()
    SamplesTheme {
        MainControls(player)
    }
}