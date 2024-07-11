package com.google.oboe.samples.liveEffect;

import android.app.ForegroundServiceStartNotAllowedException;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.widget.Toast;

import androidx.core.app.NotificationCompat;
import androidx.core.app.ServiceCompat;
import androidx.core.content.ContextCompat;

public class DuplexStreamForegroundService extends Service {
    private static final String TAG = MainActivity.class.getName();
    public static final String ACTION_START = "ACTION_START";
    public static final String ACTION_STOP = "ACTION_STOP";

    public void startForegroundService1() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            startForeground(1, buildNotification(),
                                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                                        | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
        }
    }

    public void stopForegroundService() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            stopForeground(STOP_FOREGROUND_REMOVE);
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        // We don't provide binding, so return null
        return null;
    }

    private Notification buildNotification() {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            manager.createNotificationChannel(new NotificationChannel(
                    "all",
                    "All Notifications",
                    NotificationManager.IMPORTANCE_NONE));

            return new Notification.Builder(this, "all")
                    .setContentTitle("Recording audio")
                    .setContentText("recording...")
                    .setSmallIcon(R.drawable.balance_seekbar)
                    .build();
        }
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.i(TAG, "Receive onStartCommand" + intent);
        switch (intent.getAction()) {
            case ACTION_START:
                Log.i(TAG, "Receive ACTION_START" + intent.getExtras());
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    startForeground(1, buildNotification(),
                            ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK
                                    | ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE);
                }
                break;
            case ACTION_STOP:
                Log.i(TAG, "Receive STOP_RECORD" + intent.getExtras());
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                    stopForeground(STOP_FOREGROUND_REMOVE);
                }
                break;
        }
        return START_NOT_STICKY;
    }
}
