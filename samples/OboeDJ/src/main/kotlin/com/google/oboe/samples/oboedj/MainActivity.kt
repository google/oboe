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

package com.google.oboe.samples.oboedj

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.atan2
import androidx.compose.ui.platform.LocalContext

class MainActivity : ComponentActivity() {
    private val engine = DJEngine()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        engine.init()
        
        // Load tracks from assets
        engine.loadTrack(assets, "song1.wav", 0)
        engine.loadTrack(assets, "song2.wav", 1)
        
        engine.start()

        setContent {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    background = Color(0xFF121212),
                    surface = Color(0xFF1E1E1E),
                    primary = Color.Red,           // Match standard Red from vinyl disk center
                    secondary = Color(0xFFFF4081), // Neon Pink
                    onPrimary = Color.White       // White text on red buttons
                )
            ) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    DJApp(engine)
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        engine.stop()
    }
}

@Composable
fun DJApp(engine: DJEngine) {
    var crossfader by remember { mutableFloatStateOf(0.5f) }
    var deck1Track by remember { mutableStateOf("Chemical Reaction") }
    var deck2Track by remember { mutableStateOf("Digital Noca") }
    var queuedTrack by remember { mutableStateOf("Window Seat") }

    // State to share speeds between decks for syncing
    var deck1Speed by remember { mutableFloatStateOf(128f / 170f) }
    var deck2Speed by remember { mutableFloatStateOf(1.0f) }

    var deck1Playing by remember { mutableStateOf(false) }
    var deck2Playing by remember { mutableStateOf(false) }

    val trackBpms = mapOf("Chemical Reaction" to 170f, "Digital Noca" to 128f, "Window Seat" to 100f)
    val context = LocalContext.current

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(top = 32.dp, bottom = 16.dp, start = 16.dp, end = 16.dp), // Added top padding for EdgeToEdge
        verticalArrangement = Arrangement.SpaceEvenly,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            DeckUI(
                deckIndex = 0,
                trackName = deck1Track,
                currentSpeed = deck1Speed,
                onSpeedChange = { deck1Speed = it },
                otherDeckBpm = trackBpms[deck2Track] ?: 128f,
                thisDeckBpm = trackBpms[deck1Track] ?: 170f,
                otherDeckSpeed = deck2Speed,
                isPlaying = deck1Playing,
                onPlayingChange = { deck1Playing = it },
                engine = engine
            )
            DeckUI(
                deckIndex = 1,
                trackName = deck2Track,
                currentSpeed = deck2Speed,
                onSpeedChange = { deck2Speed = it },
                otherDeckBpm = trackBpms[deck1Track] ?: 170f,
                thisDeckBpm = trackBpms[deck2Track] ?: 128f,
                otherDeckSpeed = deck1Speed,
                isPlaying = deck2Playing,
                onPlayingChange = { deck2Playing = it },
                engine = engine
            )
        }

        Column(
            modifier = Modifier.fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("Crossfader", color = Color.White)
            Slider(
                value = crossfader,
                onValueChange = {
                    crossfader = it
                    engine.setCrossfader(it)
                },
                modifier = Modifier.width(300.dp)
            )
        }

        // Queue Feature
        Column(
            modifier = Modifier.fillMaxWidth().padding(top = 16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("Next in Queue: $queuedTrack", color = Color.White, fontSize = 18.sp) // Color changed to White
            Spacer(modifier = Modifier.height(8.dp))
            Row {
                Button(onClick = {
                    val temp = deck1Track
                    deck1Track = queuedTrack
                    queuedTrack = temp
                    val filename = when (deck1Track) {
                        "Chemical Reaction" -> "song1.wav"
                        "Digital Noca" -> "song2.wav"
                        else -> "song3.wav"
                    }
                    engine.loadTrack(context.assets, filename, 0)
                    deck1Playing = true
                    engine.setPlaying(0, true)
                }) {
                    Text("Load into Deck 1")
                }
                Spacer(modifier = Modifier.width(8.dp))
                Button(onClick = {
                    val temp = deck2Track
                    deck2Track = queuedTrack
                    queuedTrack = temp
                    val filename = when (deck2Track) {
                        "Chemical Reaction" -> "song1.wav"
                        "Digital Noca" -> "song2.wav"
                        else -> "song3.wav"
                    }
                    engine.loadTrack(context.assets, filename, 1)
                    deck2Playing = true
                    engine.setPlaying(1, true)
                }) {
                    Text("Load into Deck 2")
                }
            }
        }
    }
}

@Composable
fun DeckUI(
    deckIndex: Int,
    trackName: String,
    currentSpeed: Float,
    onSpeedChange: (Float) -> Unit,
    otherDeckBpm: Float,
    thisDeckBpm: Float,
    otherDeckSpeed: Float,
    isPlaying: Boolean,
    onPlayingChange: (Boolean) -> Unit,
    engine: DJEngine
) {
    var speed by remember { mutableFloatStateOf(currentSpeed) }
    var rotationAngle by remember { mutableFloatStateOf(0.0f) }

    // Automatic rotation when playing
    LaunchedEffect(isPlaying) {
        if (isPlaying) {
            while (isPlaying) {
                rotationAngle = (rotationAngle + speed * 2f) % 360f // Visual rotation scale
                delay(16) // ~60 FPS
            }
        }
    }

    // Keep speed State in sync with prop if changed from outside (Sync button)
    LaunchedEffect(currentSpeed) {
        speed = currentSpeed
    }

    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier
            .background(MaterialTheme.colorScheme.surface)
            .padding(8.dp) // Reduced padding to prevent overlap
    ) {
        // Spacing fix: Text first, then Wheel
        Text(trackName, color = Color.White, fontSize = 14.sp)
        Spacer(modifier = Modifier.height(8.dp))

        // Turnable Vinyl Wheel
        VinylWheel(
            rotationAngle = rotationAngle,
            onAngleChanged = { deltaAngle ->
                rotationAngle = (rotationAngle + deltaAngle) % 360f
                // Map deltaAngle to scratch speed
                // 1 degree per 16ms is ~ 60 deg/sec $\approx$ 1 rad/sec.
                // Let's make it feel responsive.
                val scratchSpeed = deltaAngle / 5f // Scale factor for touch
                engine.setSpeed(deckIndex, scratchSpeed)
            },
            onRelease = {
                // Return to normal speed or stop depending on state
                engine.setSpeed(deckIndex, if (isPlaying) speed else 0.0f)
            }
        )

        Spacer(modifier = Modifier.height(16.dp))

        Row {
            Button(onClick = {
                onPlayingChange(!isPlaying)
                engine.setPlaying(deckIndex, !isPlaying)
                engine.setSpeed(deckIndex, if (!isPlaying) speed else 0.0f)
            }) {
                Text(if (isPlaying) "Pause" else "Play")
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        Text("Speed: %.2f".format(speed), color = Color.White)
        Slider(
            value = speed,
            onValueChange = {
                speed = it
                onSpeedChange(it)
                if (isPlaying) {
                    engine.setSpeed(deckIndex, it)
                }
            },
            valueRange = 0.5f..2.0f,
            modifier = Modifier.width(130.dp) // Slightly narrower
        )

        Spacer(modifier = Modifier.height(4.dp))

        Button(onClick = {
            // Sync Logic: Target Speed = (Other Deck BOM / This Deck BPM) * Other Speed
            val targetSpeed = (otherDeckBpm / thisDeckBpm) * otherDeckSpeed
            speed = targetSpeed
            onSpeedChange(targetSpeed)
            if (isPlaying) {
                engine.setSpeed(deckIndex, targetSpeed)
            }
        }) {
            Text("Sync BPМ", fontSize = 12.sp)
        }
    }
}

@Composable
fun VinylWheel(
    rotationAngle: Float,
    onAngleChanged: (Float) -> Unit,
    onRelease: () -> Unit
) {
    var lastAngle by remember { mutableFloatStateOf(0f) }

    Box(
        modifier = Modifier
            .size(160.dp)
            .clip(CircleShape)
            .background(Color.Black)
            .pointerInput(Unit) {
                detectDragGestures(
                    onDragStart = { offset ->
                        lastAngle = atan2(offset.y - size.height / 2f, offset.x - size.width / 2f)
                    },
                    onDrag = { change, _ ->
                        val currentOffset = change.position
                        val currentAngle = atan2(
                            currentOffset.y - size.height / 2f,
                            currentOffset.x - size.width / 2f
                        )
                        var delta = Math.toDegrees((currentAngle - lastAngle).toDouble()).toFloat()
                        
                        // Handle wraparound
                        if (delta > 180f) delta -= 360f
                        if (delta < -180f) delta += 360f

                        onAngleChanged(delta)
                        lastAngle = currentAngle
                    },
                    onDragEnd = {
                        onRelease()
                    },
                    onDragCancel = {
                        onRelease()
                    }
                )
            },
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.fillMaxSize()) {
            val center = Offset(size.width / 2, size.height / 2)
            val radius = size.minDimension / 2

            // Draw vinyl base
            drawCircle(
                color = Color(0xFF1A1A1A),
                radius = radius,
                center = center
            )

            // Draw grooves (concentric circles)
            for (i in 1..10) {
                drawCircle(
                    color = Color.DarkGray.copy(alpha = 0.5f),
                    radius = radius * (i / 10f),
                    center = center,
                    style = androidx.compose.ui.graphics.drawscope.Stroke(width = 1f)
                )
            }

            // Draw center label
            drawCircle(
                color = Color.Red,
                radius = radius * 0.3f,
                center = center
            )

            // Draw indicator line (so you can see it spin)
            val angleRad = Math.toRadians(rotationAngle.toDouble()).toFloat()
            val endX = center.x + radius * Math.cos(angleRad.toDouble()).toFloat()
            val endY = center.y + radius * Math.sin(angleRad.toDouble()).toFloat()
            
            drawLine(
                color = Color.White,
                start = center,
                end = Offset(endX, endY),
                strokeWidth = 4f
            )
        }
    }
}
