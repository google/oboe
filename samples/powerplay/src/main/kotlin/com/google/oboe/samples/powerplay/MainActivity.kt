/*
 * Copyright 2025 The Android Open Source Project
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

package com.google.oboe.samples.powerplay

import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.ExperimentalAnimationApi
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.LinearOutSlowInEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.with
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PageSize
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Outline
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathOperation
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Density
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.google.oboe.samples.powerplay.engine.AudioForegroundService
import com.google.oboe.samples.powerplay.engine.OboePerformanceMode
import com.google.oboe.samples.powerplay.engine.PlayerState
import com.google.oboe.samples.powerplay.engine.PowerPlayAudioPlayer
import com.google.oboe.samples.powerplay.ui.theme.MusicPlayerTheme

class MainActivity : ComponentActivity() {

    private lateinit var player: PowerPlayAudioPlayer
    private lateinit var serviceIntent: Intent
    private var isMMapSupported: Boolean = false
    private var isOffloadSupported: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setUpPowerPlayAudioPlayer()

        val format = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
            .setSampleRate(48000)
            .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
            .build()

        val attributes =
            AudioAttributes.Builder()
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build()

        serviceIntent = Intent(this, AudioForegroundService::class.java)
        isOffloadSupported = AudioManager.isOffloadedPlaybackSupported(format, attributes)
        isMMapSupported = player.isMMapSupported()

        setContent {
            MusicPlayerTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    SongScreen()
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        player.stopPlaying(0)
        player.teardownAudioStream()
    }

    private fun setUpPowerPlayAudioPlayer() {
        player = PowerPlayAudioPlayer()
        player.setupAudioStream()
    }

    /***
     * Brings together all UI elements for the player
     */
    @OptIn(ExperimentalAnimationApi::class)
    @Preview
    @Composable
    fun SongScreen() {
        val playList = getPlayList()
        val pagerState = rememberPagerState(pageCount = { playList.count() })
        val playingSongIndex = remember {
            mutableIntStateOf(0)
        }
        val offload = remember {
            mutableIntStateOf(0) // 0: None, 1: Low Latency, 2: Power Saving, 3: PCM Offload
        }

        val isMMapEnabled = remember { mutableStateOf(player.isMMapEnabled()) }

        LaunchedEffect(pagerState.currentPage) {
            playingSongIndex.intValue = pagerState.currentPage
        }

        LaunchedEffect(Unit) {
            playList.forEachIndexed { index, it ->
                player.loadFile(assets, it.fileName, index)
                player.setLooping(index, true)
            }
        }

        val isPlaying = remember {
            mutableStateOf(false)
        }

        Box(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center
        ) {
            val configuration = LocalConfiguration.current

            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                AnimatedContent(targetState = playingSongIndex.intValue, transitionSpec = {
                    (scaleIn() + fadeIn()) with (scaleOut() + fadeOut())
                }, label = "") {
                    Text(
                        text = playList[it].name, fontSize = 24.sp,
                        color = Color.Black,
                        style = TextStyle(fontWeight = FontWeight.ExtraBold)
                    )
                }
                Spacer(modifier = Modifier.height(8.dp))
                AnimatedContent(targetState = playingSongIndex.intValue, transitionSpec = {
                    (scaleIn() + fadeIn()) with (scaleOut() + fadeOut())
                }, label = "") {
                    Text(
                        text = playList[it].artist, fontSize = 12.sp, color = Color.Black,
                        style = TextStyle(fontWeight = FontWeight.Bold)
                    )
                }

                Spacer(modifier = Modifier.height(16.dp))

                /***
                 * Includes animated song album cover
                 */
                HorizontalPager(
                    modifier = Modifier.fillMaxWidth(),
                    state = pagerState,
                    pageSize = PageSize.Fixed((configuration.screenWidthDp / (1.7)).dp),
                    contentPadding = PaddingValues(horizontal = 85.dp)
                ) { page ->
                    val painter = painterResource(id = playList[page].cover)
                    if (page == pagerState.currentPage) {
                        VinylAlbumCoverAnimation(isSongPlaying = isPlaying.value, painter = painter)
                    } else {
                        VinylAlbumCoverAnimation(isSongPlaying = false, painter = painter)
                    }
                }
                Spacer(modifier = Modifier.height(54.dp))
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 32.dp),
                ) {
                }
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    "Performance Modes"
                )
                Spacer(modifier = Modifier.height(8.dp))

                Column {
                    val radioOptions = mutableListOf("None", "Low Latency", "Power Saving")
                    if (isOffloadSupported) radioOptions.add("PCM Offload")

                    val (selectedOption, onOptionSelected) = remember {
                        mutableStateOf(radioOptions[0])
                    }
                    val enabled = !isPlaying.value
                    radioOptions.forEachIndexed { index, text ->
                        Row(
                            Modifier
                                .height(32.dp)
                                .selectable(
                                    selected = (text == selectedOption),
                                    enabled = enabled,
                                    onClick = {
                                        if (enabled) {
                                            onOptionSelected(text)
                                            offload.intValue = index
                                            player.teardownAudioStream()
                                        }
                                    },
                                    role = Role.RadioButton
                                )
                                .padding(horizontal = 4.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            RadioButton(
                                selected = (text == selectedOption),
                                onClick = null,
                                enabled = enabled
                            )
                            Text(
                                text = text,
                                style = MaterialTheme.typography.bodyLarge,
                                modifier = Modifier.padding(start = 8.dp)
                            )
                        }
                    }
                }
                Spacer(modifier = Modifier.height(8.dp))
                Text(
                    when (offload.intValue) {
                        0 -> "Performance Mode: None"
                        1 -> "Performance Mode: Low Latency"
                        2 -> "Performance Mode: Power Saving"
                        else -> "Performance Mode: PCM Offload"
                    }
                )
                Spacer(modifier = Modifier.height(8.dp))
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 32.dp)
                        .padding(vertical = 4.dp)
                ) {
                    if (isMMapSupported) {
                        Checkbox(
                            checked = !isMMapEnabled.value,
                            onCheckedChange = {
                                if (!isPlaying.value) {
                                    isMMapEnabled.value = !it
                                    player.setMMapEnabled(isMMapEnabled.value)
                                }
                            },
                            enabled = !isPlaying.value
                        )
                        Text(
                            text = "Disable MMAP",
                            style = MaterialTheme.typography.bodyLarge,
                            modifier = Modifier.padding(start = 8.dp)
                        )

                    }
                    Text(
                        text = when (isMMapEnabled.value) {
                            true -> "| Current Mode: MMAP"
                            false -> "| Current Mode: Classic"
                        },
                        style = MaterialTheme.typography.bodyLarge,
                        modifier = Modifier.padding(start = 8.dp)
                    )
                }
                Spacer(modifier = Modifier.height(24.dp))
                Row(
                    horizontalArrangement = Arrangement.SpaceEvenly,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Spacer(modifier = Modifier.width(20.dp))
                    ControlButton(
                        icon = if (isPlaying.value) R.drawable.ic_pause else R.drawable.ic_play,
                        size = 100.dp,
                        onClick = {
                            when (isPlaying.value) {
                                true -> player.stopPlaying(playingSongIndex.intValue)
                                false -> {
                                    player.startPlaying(
                                        playingSongIndex.intValue,
                                        OboePerformanceMode.fromInt(offload.intValue)
                                    )
                                }
                            }

                            isPlaying.value =
                                player.getPlayerStateLive().value == PlayerState.Playing
                        })
                    Spacer(modifier = Modifier.width(20.dp))
                }
            }
        }
    }
    @Composable
    fun ControlButton(icon: Int, size: Dp, onClick: () -> Unit) {
        Box(
            modifier = Modifier
                .size(size)
                .clip(CircleShape)
                .clickable {
                    onClick()
                }, contentAlignment = Alignment.Center
        ) {
            Icon(
                modifier = Modifier.size(size / 1.5f),
                painter = painterResource(id = icon),
                tint = Color.Black,
                contentDescription = null
            )
        }
    }

    @Composable
    fun VinylAlbumCoverAnimation(
        modifier: Modifier = Modifier,
        isSongPlaying: Boolean = true,
        painter: Painter
    ) {
        var currentRotation by remember {
            mutableFloatStateOf(0f)
        }

        val rotation = remember {
            Animatable(currentRotation)
        }

        LaunchedEffect(isSongPlaying) {
            if (isSongPlaying) {
                rotation.animateTo(
                    targetValue = currentRotation + 360f,
                    animationSpec = infiniteRepeatable(
                        animation = tween(3000, easing = LinearEasing),
                        repeatMode = RepeatMode.Restart
                    )
                ) {
                    currentRotation = value
                }
            } else {
                if (currentRotation > 0f) {
                    rotation.animateTo(
                        targetValue = currentRotation + 50,
                        animationSpec = tween(
                            1250,
                            easing = LinearOutSlowInEasing
                        )
                    ) {
                        currentRotation = value
                    }
                }
            }
        }

        VinylAlbumCover(
            painter = painter,
            rotationDegrees = rotation.value
        )
    }

    @Composable
    fun VinylAlbumCover(
        modifier: Modifier = Modifier,
        rotationDegrees: Float = 0f,
        painter: Painter
    ) {

        /**
         * Creates a custom outline for a rounded shape
         */
        val roundedShape = object : Shape {
            override fun createOutline(
                size: Size,
                layoutDirection: LayoutDirection,
                density: Density
            ): Outline {
                val p1 = Path().apply {
                    addOval(Rect(4f, 3f, size.width - 1, size.height - 1))
                }
                val thickness = size.height / 2.10f
                val p2 = Path().apply {
                    addOval(
                        Rect(
                            thickness,
                            thickness,
                            size.width - thickness,
                            size.height - thickness
                        )
                    )
                }
                val p3 = Path()
                p3.op(p1, p2, PathOperation.Difference)

                return Outline.Generic(p3)
            }
        }

        /**
         * Container defining the layout for a vinyl-themed UI element.
         */
        Box(
            modifier = modifier
                .aspectRatio(1.0f)
                .clip(roundedShape)
        ) {

            /**
             * Vinyl background image
             */
            Image(
                modifier = Modifier
                    .fillMaxSize()
                    .rotate(rotationDegrees),
                painter = painterResource(id = R.drawable.vinyl_background),
                contentDescription = "Vinyl Background"
            )


            /**
             * Song album cover image overlaid on the vinyl background image
             */
            Image(
                modifier = Modifier
                    .fillMaxSize(0.5f)
                    .rotate(rotationDegrees)
                    .aspectRatio(1.0f)
                    .align(Alignment.Center)
                    .clip(roundedShape),
                painter = painter,
                contentDescription = "Song cover"
            )
        }
    }

    /***
     * Convert the millisecond to String text
     */
    private fun Long.convertToText(): String {
        val sec = this / 1000
        val minutes = sec / 60
        val seconds = sec % 60

        val minutesString = if (minutes < 10) {
            "0$minutes"
        } else {
            minutes.toString()
        }
        val secondsString = if (seconds < 10) {
            "0$seconds"
        } else {
            seconds.toString()
        }
        return "$minutesString:$secondsString"
    }


    /***
     * Return a play list of type Music data class
     */
    private fun getPlayList(): List<Music> {
        return listOf(
            Music(
                name = "Chemical Reaction",
                artist = "Momo Oboe",
                cover = R.drawable.album_art_1,
                fileName = "song1.wav",
            ),
        )
    }

    /***
     * Data class to represent a music in the list
     */
    data class Music(
        val name: String,
        val artist: String,
        val fileName: String,
        val cover: Int,
    )
}
