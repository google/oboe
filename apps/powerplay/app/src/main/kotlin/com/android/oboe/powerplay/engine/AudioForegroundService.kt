package com.android.oboe.powerplay.engine

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.android.oboe.powerplay.MainActivity
import com.android.oboe.powerplay.R

class AudioForegroundService : Service() {

    private lateinit var audioManager: AudioManager
    private lateinit var audioFocusRequest: AudioFocusRequest
    private val audioFocusChangeListener = AudioManager.OnAudioFocusChangeListener { focusChange ->
        when (focusChange) {
            AudioManager.AUDIOFOCUS_GAIN -> {

            }

            AudioManager.AUDIOFOCUS_LOSS, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        audioManager = getSystemService(AUDIO_SERVICE) as AudioManager
        val audioAttributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_MEDIA)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build()

        audioFocusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(audioAttributes)
            .setAcceptsDelayedFocusGain(true)
            .setOnAudioFocusChangeListener(audioFocusChangeListener)
            .build()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val notification = createNotification()
        startForeground(NOTIFICATION_ID, notification)
        val result = audioManager.requestAudioFocus(audioFocusRequest)

        if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            // Start playback only when audio focus is granted
            // ...
        }

        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        audioManager.abandonAudioFocusRequest(audioFocusRequest)
    }

    private fun createNotification(): Notification {
        val channelId = createNotificationChannel()
        val notificationIntent =
            Intent(this, MainActivity::class.java) // Replace with your main activity
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            notificationIntent,
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, channelId)
            .setContentTitle("Audio Playing")
            .setContentText("Audio is playing in the background")
            .setSmallIcon(R.drawable.ic_play) // Replace with your icon
            .setContentIntent(pendingIntent)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun createNotificationChannel(): String {
        val channelId = "audio_playback_channel"
        val channelName = "Audio Playback"
        val importance = NotificationManager.IMPORTANCE_LOW
        val channel = NotificationChannel(channelId, channelName, importance)
        val notificationManager =
            getSystemService(NOTIFICATION_SERVICE) as NotificationManager
        notificationManager.createNotificationChannel(channel)
        return channelId
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null // Not using binding in this example
    }

    companion object {
        private const val NOTIFICATION_ID = 1
    }
}