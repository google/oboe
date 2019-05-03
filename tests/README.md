Before running the tests you must define $ANDROID_NDK
and make sure cmake is in the PATH.

To test this on Mac or Linux enter:

    echo $ANDROID_HOME
    echo $ANDROID_NDK
    cmake --version

If you need to set ANDROID_NDK then this may work on Mac OS:

    export ANDROID_HOME=$HOME/Library/Android/sdk
    export ANDROID_NDK=$ANDROID_HOME/ndk-bundle
    
This may work on Linux:

    export ANDROID_HOME=$HOME/Android/Sdk
    export ANDROID_NDK=$ANDROID_HOME/ndk-bundle

If you need to add cmake to the path then you can find it by entering:

    ls $ANDROID_HOME/cmake
    
Make note of the folder name. Mine was "3.6.4111459" so I entered:
    
    export PATH=$PATH:$ANDROID_HOME/cmake/3.6.4111459/bin
    cmake --version
    
Then to run the tests, enter:

    cd tests
    ./run_tests.sh
    
You may need to enter \<control-c\> to exit the script.

If you get this error:

    com.android.builder.testing.api.DeviceException: com.android.ddmlib.InstallException:
        INSTALL_FAILED_UPDATE_INCOMPATIBLE: Package com.google.oboe.tests.unittestrunner
        signatures do not match previously installed version; ignoring!

then uninstall the app "UnitTestRunner" from the Android device.

See run_tests.sh for more documentation
