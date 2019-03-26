package com.google.oboe.tests.unittestrunner;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

public class MainActivity extends AppCompatActivity {

    private final String TAG = MainActivity.class.getName();
    private static final String TEST_BINARY_FILEANAME = "testOboe";
    private static final int APP_PERMISSION_REQUEST = 0;

    private TextView outputText;
    private ScrollView scrollView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        outputText = findViewById(R.id.output_view_text);
        scrollView = findViewById(R.id.scroll_view);
        runCommand();
    }

    private void runCommand(){
        if (!isRecordPermissionGranted()){
            requestPermissions();
        } else {

            Thread commandThread = new Thread(new UnitTestCommand());
            commandThread.start();
        }
    }

    private String executeBinary() {

        AssetManager assetManager = getAssets();

        StringBuffer output = new StringBuffer();
        String abi = Build.CPU_ABI;
        String filesDir = getFilesDir().getPath();
        String testBinaryPath = abi + "/" + TEST_BINARY_FILEANAME;

        try {
            InputStream inStream = assetManager.open(testBinaryPath);
            Log.d(TAG, "Opened " + testBinaryPath);

            // Copy this file to an executable location
            File outFile = new File(filesDir, TEST_BINARY_FILEANAME);

            OutputStream outStream = new FileOutputStream(outFile);

            byte[] buffer = new byte[1024];
            int read;
            while ((read = inStream.read(buffer)) != -1) {
                outStream.write(buffer, 0, read);
            }
            inStream.close();
            outStream.flush();
            outStream.close();
            Log.d(TAG, "Copied " + testBinaryPath + " to " + filesDir);

            String executablePath =  filesDir + "/" + TEST_BINARY_FILEANAME;
            Log.d(TAG, "Attempting to execute " + executablePath);

            new File(executablePath).setExecutable(true, false);
            Log.d(TAG, "Setting execute permission on " + executablePath);

            Process process = Runtime.getRuntime().exec(executablePath);

            BufferedReader stdInput = new BufferedReader(new
                    InputStreamReader(process.getInputStream()));

            BufferedReader stdError = new BufferedReader(new
                    InputStreamReader(process.getErrorStream()));

            // read the output from the command
            String s = null;
            while ((s = stdInput.readLine()) != null) {
                Log.d(TAG, s);
                output.append(s + "\n");
            }

            // read any errors from the attempted command
            while ((s = stdError.readLine()) != null) {
                Log.e(TAG, "ERROR: " + s);
                output.append("ERROR: " + s + "\n");
            }

            process.waitFor();
            Log.d(TAG, "Finished executing binary");
        } catch (IOException e){
            Log.e(TAG, "Could not execute binary ", e);
        } catch (InterruptedException e) {
            Log.e(TAG, "Interrupted", e);
        }

        return output.toString();
    }

    private boolean isRecordPermissionGranted() {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
                PackageManager.PERMISSION_GRANTED);
    }

    private void requestPermissions(){
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.RECORD_AUDIO},
                APP_PERMISSION_REQUEST);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions,
                                           @NonNull int[] grantResults) {

        if (APP_PERMISSION_REQUEST != requestCode) {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
            return;
        }

        if (grantResults.length != 1 ||
                grantResults[0] != PackageManager.PERMISSION_GRANTED) {

            // User denied the permission, without this we cannot record audio
            // Show a toast and update the status accordingly
            outputText.setText(R.string.status_record_audio_denied);
            Toast.makeText(getApplicationContext(),
                    getString(R.string.need_record_audio_permission),
                    Toast.LENGTH_SHORT)
                    .show();
        } else {
            // Permission was granted, run the command
            runCommand();
        }
    }

    class UnitTestCommand implements Runnable {

        @Override
        public void run() {
            final String output = executeBinary();

            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    outputText.setText(output);

                    // Scroll to the bottom so we can see the test result
                    scrollView.postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            scrollView.scrollTo(0, outputText.getBottom());
                        }
                    }, 100);
                }
            });
        }
    }
}
