package com.mobileer.oboetester;

import android.content.Intent;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;

public class ExtraTestsActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_extra_tests);
    }

    public void onLaunchMainActivity(View view) {
        onLaunchTest(MainActivity.class);
    }

    public void onLaunchExternalTapTest(View view) {
        onLaunchTest(ExternalTapToToneActivity.class);
    }

    private void onLaunchTest(Class clazz) {
        Intent intent = new Intent(this, clazz);
        startActivity(intent);
    }
}
