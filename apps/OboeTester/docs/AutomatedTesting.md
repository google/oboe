# Automated Testing

DRAFT - not yet implemented, just for design review

You can use an Android Intent to run the OboeTester app from a shell script.
This can be used when you do not have a rooted device and cannot use the command line executable.

## Start App from Intent

The app can be started by sending a Start comment to the OboeTester class.
The app will run and the results will be written to a file.

    adb shell am start -n com.google.sample.oboe.manualtest/.MainActivity {parameters}
    
String parameters are sent using:

    --es {parameterName} {parameterValue}

For example:

    --es test latency

Integer parameters are sent using:

    --ei {parameterName} {parameterValue}
    
For example:

    --ei buffer_size 32

## Parameters

There are two required parameters:

    --es test {latency, glitch, or auto}
    --es file {full path for resulting file}
    
There are several optional parameters.

    --ei in_sample_rate     {hertz}
    --ei in_channels        {samples}
    --ei out_sample_rate    {hertz}
    --ei out_channels       {samples}
    --ei duration           {seconds}
    --ei buffer_size        {bursts}

For example, a complete command might be:

    adb shell am start -n com.google.sample.oboe.manualtest/.MainActivity \
        --es test latency \
        --es file /sdcard/test20190611.txt \
        --ei buffer_size 1
        
        
        
