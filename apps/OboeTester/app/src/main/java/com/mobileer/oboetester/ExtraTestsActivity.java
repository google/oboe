package com.mobileer.oboetester;

import android.content.Intent;
import android.app.Activity;
import android.os.Bundle;
import android.view.View;

public class ExtraTestsActivity extends BaseOboeTesterActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_extra_tests);
    }

    public void onLaunchMainActivity(View view) {
        launchTestActivity(MainActivity.class);
    }

    public void onLaunchExternalTapTest(View view) {
        launchTestThatDoesRecording(ExternalTapToToneActivity.class);
    }

}
