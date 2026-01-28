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

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.os.Bundle
import android.os.IBinder
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedVisibility
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
import androidx.compose.animation.togetherWith
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
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PageSize
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.RadioButton
import androidx.compose.material3.RadioButtonDefaults
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.TextButton
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Info
import androidx.compose.foundation.background
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.border
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.livedata.observeAsState
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshotFlow
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
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
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.distinctUntilChanged

class MainActivity : ComponentActivity() {

    private lateinit var player: PowerPlayAudioPlayer
    private lateinit var serviceIntent: Intent
    private var isMMapSupported: Boolean = false
    private var isOffloadSupported: Boolean = false
    private var sampleRate: Int = 48000
    private var isBound = mutableStateOf(false)

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(className: ComponentName, service: IBinder) {
            val binder = service as AudioForegroundService.LocalBinder
            player = binder.getService().player
            isMMapSupported = player.isMMapSupported()
            isBound.value = true
        }

        override fun onServiceDisconnected(arg0: ComponentName) {
            isBound.value = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setUpPowerPlayAudioPlayer()

        val format = AudioFormat.Builder()
            .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
            .setSampleRate(sampleRate)
            .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
            .build()

        val attributes =
            AudioAttributes.Builder()
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build()

        serviceIntent = Intent(this, AudioForegroundService::class.java)
        isOffloadSupported = AudioManager.isOffloadedPlaybackSupported(format, attributes)

        setContent {
            MusicPlayerTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    if (isBound.value) {
                        SongScreen()
                    } else {
                        Box(contentAlignment = Alignment.Center, modifier = Modifier.fillMaxSize()) {
                            CircularProgressIndicator()
                        }
                    }
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        if (isBound.value) {
            unbindService(connection)
            isBound.value = false
        }
    }

    private fun setUpPowerPlayAudioPlayer() {
        val intent = Intent(this, AudioForegroundService::class.java)
        startForegroundService(intent) // Starts the service
        bindService(intent, connection, Context.BIND_AUTO_CREATE)
    }

    /***
     * Brings together all UI elements for the player
     */
    @OptIn(ExperimentalAnimationApi::class, ExperimentalMaterial3Api::class)
    @Preview
    @Composable
    fun SongScreen() {
        val playList = PlayList
        val initialPage = remember { player.currentSongIndex }
        val pagerState = rememberPagerState(initialPage = initialPage, pageCount = { playList.count() })
        val playingSongIndex = remember {
            mutableIntStateOf(initialPage)
        }
        val offload = remember {
            mutableIntStateOf(player.currentPerformanceMode.ordinal)
        }

        val isMMapEnabled = remember { mutableStateOf(player.isMMapEnabled()) }
        val playerStateWrapper = player.getPlayerStateLive().observeAsState(PlayerState.NoResultYet)
        val isPlaying = playerStateWrapper.value == PlayerState.Playing
        var sliderPosition by remember { mutableFloatStateOf(0f) }

        var showBottomSheet by remember { mutableStateOf(false) }
        val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
        
        var showInfoDialog by remember { mutableStateOf(false) }

        LaunchedEffect(pagerState) {
            snapshotFlow { pagerState.currentPage }
                .distinctUntilChanged()
                .collect { page ->
                    playingSongIndex.intValue = pagerState.currentPage
                    // Check the latest value of playerState state object
                    if (playerStateWrapper.value == PlayerState.Playing) {
                        player.startPlaying(
                            playingSongIndex.intValue,
                            OboePerformanceMode.fromInt(offload.intValue)
                        )
                    }
                }
        }

        LaunchedEffect(Unit) {
            playList.forEachIndexed { index, it ->
                player.loadFile(assets, it.fileName, index)
                player.setLooping(index, true)
            }
        }

        Box(
            modifier = Modifier
                .fillMaxSize()
                .windowInsetsPadding(WindowInsets.safeDrawing),
            contentAlignment = Alignment.Center
        ) {
            val configuration = LocalConfiguration.current
            IconButton(
                onClick = { showBottomSheet = true },
                modifier = Modifier
                    .align(Alignment.BottomEnd)
                    .padding(32.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Settings,
                    contentDescription = "Performance Settings",
                    tint = Color.Black,
                    modifier = Modifier.size(32.dp)
                )
            }
            
            IconButton(
                onClick = { showInfoDialog = true },
                modifier = Modifier
                    .align(Alignment.BottomStart)
                    .padding(32.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Info,
                    contentDescription = "Audio Info",
                    tint = Color.Black,
                    modifier = Modifier.size(32.dp)
                )
            }

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
                    (scaleIn() + fadeIn()).togetherWith(scaleOut() + fadeOut())
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
                    contentPadding = PaddingValues(horizontal = 85.dp),
                ) { page ->
                    val painter = painterResource(id = playList[page].cover)
                    if (page == pagerState.currentPage) {
                        VinylAlbumCoverAnimation(isSongPlaying = isPlaying, painter = painter)
                    } else {
                        VinylAlbumCoverAnimation(isSongPlaying = false, painter = painter)
                    }
                }
                Spacer(modifier = Modifier.height(24.dp))
                Row(
                    horizontalArrangement = Arrangement.SpaceEvenly,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Spacer(modifier = Modifier.width(20.dp))
                    ControlButton(
                        icon = if (isPlaying) R.drawable.ic_pause else R.drawable.ic_play,
                        size = 100.dp,
                        onClick = {
                            when (isPlaying) {
                                true -> player.stopPlaying(playingSongIndex.intValue)
                                false -> {
                                    player.startPlaying(
                                        playingSongIndex.intValue,
                                        OboePerformanceMode.fromInt(offload.intValue)
                                    )
                                }
                            }
                        })
                    Spacer(modifier = Modifier.width(20.dp))
                }
            }
        }

        if (showBottomSheet) {
            ModalBottomSheet(
                onDismissRequest = { },
                sheetState = sheetState,
                containerColor = Color.White,
                shape = androidx.compose.foundation.shape.RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp)
            ) {
                PerformanceBottomSheetContent(
                    offload = offload,
                    isMMapEnabled = isMMapEnabled,
                    isPlaying = isPlaying,
                    sliderPosition = sliderPosition,
                    onSliderPositionChange = { sliderPosition = it },
                    onDismiss = { }
                )
            }
        }
        
        if (showInfoDialog) {
            val performanceModeText = when (offload.intValue) {
                0 -> "None"
                1 -> "Low Latency"
                2 -> "Power Saving"
                else -> "PCM Offload"
            }
            val mmapModeText = if (isMMapEnabled.value) "MMAP" else "Classic"
            val bufferInfo = if (offload.intValue == 3) {
                val bufferSeconds = sliderPosition.toDouble() / sampleRate
                "%.3f seconds".format(bufferSeconds)
            } else {
                "N/A (not in PCM Offload mode)"
            }
            
            AlertDialog(
                onDismissRequest = { },
                title = {
                    Text(
                        text = "Audio Settings Info",
                        fontWeight = FontWeight.Bold
                    )
                },
                text = {
                    Column(
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        Row {
                            Text("Performance Mode: ", fontWeight = FontWeight.Medium)
                            Text(performanceModeText)
                        }
                        Row {
                            Text("Audio Mode: ", fontWeight = FontWeight.Medium)
                            Text(mmapModeText)
                        }
                        Row {
                            Text("Buffer Size: ", fontWeight = FontWeight.Medium)
                            Text(bufferInfo)
                        }
                    }
                },
                confirmButton = {
                    TextButton(onClick = { }) {
                        Text("Close")
                    }
                },
                containerColor = Color.White,
                titleContentColor = Color.Black,
                textContentColor = Color.Black
            )
        }
    }

    /**
     * Bottom sheet content for Performance Modes settings
     */
    @Composable
    fun PerformanceBottomSheetContent(
        offload: androidx.compose.runtime.MutableIntState,
        isMMapEnabled: androidx.compose.runtime.MutableState<Boolean>,
        isPlaying: Boolean,
        sliderPosition: Float,
        onSliderPositionChange: (Float) -> Unit,
        onDismiss: () -> Unit
    ) {
        var localSliderPosition by remember { mutableFloatStateOf(sliderPosition) }
        val requestedFrames = remember { mutableIntStateOf(0) }
        val actualFrames = remember { mutableIntStateOf(0) }
        
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
                .padding(bottom = 32.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "Performance Modes",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                color = Color.Black,
                modifier = Modifier.padding(bottom = 16.dp)
            )
            Column(
                modifier = Modifier.fillMaxWidth()
            ) {
                val radioOptions = mutableListOf("None", "Low Latency", "Power Saving")
                if (isOffloadSupported) radioOptions.add("PCM Offload")

                val (selectedOption, onOptionSelected) = remember {
                    mutableStateOf(radioOptions[offload.intValue])
                }
                val enabled = !isPlaying
                radioOptions.forEachIndexed { index, text ->
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .height(48.dp)
                            .selectable(
                                selected = (text == selectedOption),
                                enabled = enabled,
                                onClick = {
                                    if (enabled) {
                                        onOptionSelected(text)
                                        player.updatePerformanceMode(OboePerformanceMode.fromInt(index))
                                        offload.intValue = index
                                    }
                                },
                                role = Role.RadioButton
                            )
                            .padding(horizontal = 8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        RadioButton(
                            selected = (text == selectedOption),
                            onClick = null,
                            enabled = enabled,
                            colors = RadioButtonDefaults.colors(
                                selectedColor = MaterialTheme.colorScheme.primary
                            )
                        )
                        Text(
                            text = text,
                            style = MaterialTheme.typography.bodyLarge,
                            modifier = Modifier.padding(start = 12.dp)
                        )
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(12.dp))
            Text(
                text = when (offload.intValue) {
                    0 -> "Performance Mode: None"
                    1 -> "Performance Mode: Low Latency"
                    2 -> "Performance Mode: Power Saving"
                    else -> "Performance Mode: PCM Offload"
                },
                color = Color.Gray,
                style = MaterialTheme.typography.bodyMedium
            )
            
            Spacer(modifier = Modifier.height(16.dp))
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth()
            ) {
                if (isMMapSupported) {
                    Checkbox(
                        checked = !isMMapEnabled.value,
                        onCheckedChange = {
                            if (!isPlaying) {
                                isMMapEnabled.value = !it
                                player.setMMapEnabled(isMMapEnabled.value)
                            }
                        },
                        enabled = !isPlaying
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
                    color = Color.Gray,
                    modifier = Modifier.padding(start = 8.dp)
                )
            }
            
            AnimatedVisibility(
                visible = offload.intValue == 3,
                enter = androidx.compose.animation.expandVertically() + fadeIn(),
                exit = androidx.compose.animation.shrinkVertically() + fadeOut()
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Spacer(modifier = Modifier.height(16.dp))
                    
                    Slider(
                        value = localSliderPosition,
                        onValueChange = { newValue ->
                            localSliderPosition = newValue
                            requestedFrames.intValue = localSliderPosition.toInt()
                            onSliderPositionChange(newValue)
                        },
                        onValueChangeFinished = {
                            requestedFrames.intValue = localSliderPosition.toInt()
                            actualFrames.intValue = player.setBufferSizeInFrames(requestedFrames.intValue)
                        },
                        valueRange = 0f..player.getBufferCapacityInFrames().toFloat(),
                        colors = SliderDefaults.colors(
                            thumbColor = MaterialTheme.colorScheme.primary,
                            activeTrackColor = MaterialTheme.colorScheme.primary
                        ),
                        modifier = Modifier.fillMaxWidth()
                    )

                    val actualSeconds = actualFrames.intValue.toDouble() / sampleRate
                    val formattedSeconds = "%.3f".format(actualSeconds)

                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Text(
                            text = "Requested: ${requestedFrames.intValue} Frames (BufferSize)",
                            color = Color.Black,
                            style = MaterialTheme.typography.bodySmall
                        )
                        Text(
                            text = "Actual: ${actualFrames.intValue} Frames ($formattedSeconds seconds)",
                            color = Color.Black,
                            style = MaterialTheme.typography.bodySmall
                        )
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(24.dp))
            Box(
                modifier = Modifier
                    .size(48.dp)
                    .clip(CircleShape)
                    .background(Color(0xFFF0F0F0))
                    .clickable { onDismiss() },
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Default.Close,
                    contentDescription = "Close",
                    tint = Color.DarkGray,
                    modifier = Modifier.size(24.dp)
                )
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

}
