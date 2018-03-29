package com.donturner.rhythmgame;

import android.content.res.AssetManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.MotionEvent;
import android.view.WindowManager;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        native_onCreate(getAssets());
    }

    private native void native_onCreate(AssetManager assetManager);

    //@Override
    /*public boolean onTouchEvent(MotionEvent event) {
        native_onTouchEvent(event);
        return true;
    }*/

    //private native void native_onTouchEvent(MotionEvent event);
}
