# Automated Testing

(Note: the Latency test works. The Glitch test is not yet implemented.)

You can use an Android Intent to run the OboeTester app from a shell script.
This can be used when you do not have a rooted device and, therefore, cannot use the command line executable.

Before running the app from an Intent, it should be launched manually and a Round Trip Latency  test run. Then you can give permission for using the microphone to record the looped back sound, and give permission to write to external storage for saving the test result.

## Requirements

* host computer
* ADB USB cable
* [loopback adapter](https://source.android.com/devices/audio/latency/loopback)
* a USB switching device such as [TigerTail](https://go/tigertail)

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

    --ei buffer_bursts 8

## Parameters

There are two required parameters:

    --es test {latency, glitch}
            The "latency" test will perform a Round Trip Latency test.
            It will request EXCLUSIVE mode for minimal latency.
            The "glitch" test will perform a Single Glitch test. UNIMPLEMENTED
    --es file {full path for resulting file}
    
There are several optional parameters.

    --ei buffer_bursts      {bursts}     // number of bursts in the buffer, 2 for "double buffered"
    --ei in_sample_rate     {hertz}      // UNIMPLEMENTED
    --ei in_channels        {samples}    // UNIMPLEMENTED
    --ei out_sample_rate    {hertz}      // UNIMPLEMENTED
    --ei out_channels       {samples}    // UNIMPLEMENTED
    --ei duration           {seconds}    // UNIMPLEMENTED glitch test duration, default is 10 seconds

For example, a complete command might be:

    adb shell am start -n com.google.sample.oboe.manualtest/.MainActivity \
        --es test latency \
        --es file /sdcard/test20190611.txt \
        --ei buffer_bursts 2
        
## Interpreting Test Results

Test results are simple files with "name = value" pairs.
The test results can be obtained using adb pull.

    adb pull /sdcard/test20190611.txt .
    
### Latency Report

Here is an example report:

    burst.frames =  96
    buffer.size.frames = 96
    buffer.capacity.frames = 3072
    rms.signal = 0.73868
    rms.noise = 0.00103
    reset.count = 3
    result = 0
    result.text = OK
    latency.empty.frames =    533
    latency.empty.msec =  11.10
    latency.frames =    629
    latency.msec =  13.10
    confidence =  0.887

Explanation of report values:

    burst.frames         size of a DSP burst in frames
    buffer.size.frames   output buffer size in frames
    buffer.capacity.frames maximum capacity in frames
    sample.rate          sample rate in Hertz
    rms.signal           Root Mean Square of the signal, if it can be detected
    rms.noise            Root Mean Square of the background noise before the signal is detected
    reset.count          # of times the full duplex stream input underflowed and had to resyncronize
    result               0 or a negative error
    result.text          text equivalent of the result
    latency.empty.frames round trip latency if the top output buffer was empty
    latency.empty.msec   same but translated to milliseconds
    latency.frames       round trip latency as measured
    latency.msec         same but translated to milliseconds, "pro-audioo" devices should be <=20 msec
    confidence           quality of the latency result between 0.0 and 1.0, higher is better
        
        
