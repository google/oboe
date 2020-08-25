[Home](README.md)

# Automated Testing

OboeTester can be used to measure the round trip latency and glitches. 
It can be launched from a shell script by using an Android Intent.

Before running the app from an Intent, it should be launched manually and a Round Trip Latency test run. Then you can give permission for using the microphone to record the looped back sound, and give permission to write to external storage for saving the test result.

## Requirements

* host computer
* ADB USB cable
* [loopback adapter](https://source.android.com/devices/audio/latency/loopback)
* a 3.5 mm jack on the phone*

\* If you don't have a 3.5 mm jack then you can use a USB-C to 3.5mm adapter. Then you will also need a USB switching device such as [TigerTail](https://go/tigertail).

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
            The "glitch" test will perform a single Glitch test.
    --es file {full path for resulting file}
    
There is one optional parameter for the "latency" test.

    --ei buffer_bursts      {bursts}     // number of bursts in the buffer, 2 for "double buffered"
    
There are several optional parameters for the "glitch" test:

    --ei buffer_bursts      {bursts}     // number of bursts in the buffer, 2 for "double buffered"
    --ei sample_rate        {hertz}
    --ef tolerance          {tolerance}  // amount of deviation from expected that is considered a glitch
                                         // Range of tolerance is 0.0 to 1.0. Default is 0.1. Note use of "-ef".
    --ei duration           {seconds}    // glitch test duration, default is 10 seconds
    --ei in_channels        {samples}    // number of input channels, default is 2
    --ei out_channels       {samples}    // number of output channels, default is 2
    --es in_perf            {"none", "lowlat", "powersave"}  // input performance mode, default is "lowlat"
    --es out_perf           {"none", "lowlat", "powersave"}  // output performance mode, default is "lowlat"
                            // input preset, default is "voicerec"
    --es in_preset          ("generic", "camcorder", "voicerec", "voicecomm", "unprocessed", "performance"}
    --es in_sharing         {"shared", "exclusive"} // input sharing mode, default is "exclusive"
    --es out_sharing        {"shared", "exclusive"} // output sharing mode, default is "exclusive"

For example, a complete command might be:

    adb shell am start -n com.google.sample.oboe.manualtest/.MainActivity \
        --es test latency \
        --es file /sdcard/latency20190903.txt \
        --ei buffer_bursts 2
        
or for a "glitch" test:

    adb shell am start -n com.google.sample.oboe.manualtest/.MainActivity \
        --es test glitch \
        --es file /sdcard/glitch20190903.txt \
        --es in_perf lowlat \
        --es out_perf lowlat \
        --es in_sharing exclusive \
        --es out_sharing exclusive \
        --ei buffer_bursts 2 \
        --ei sample_rate 48000 \
        --ef tolerance 0.123 \
        --ei in_channels 2 \
        --ei out_channels 2 

## Interpreting Test Results

Test results are simple files with "name = value" pairs.
The test results can be obtained using adb pull.

    adb pull /sdcard/test20190611.txt .
    
The beginning of the report is common to all tests:

    build.fingerprint = google/bonito/bonito:10/QP1A.190711.017/5771233:userdebug/dev-keys
    test.version = 1.5.10
    test.version.code = 19
    time.millis = 1567521906542
    in.channels = 2
    in.perf = lowlat
    in.sharing = exclusive
    in.api = aaudio
    in.rate = 48000
    in.device = 30
    in.mmap = yes
    in.burst.frames = 96
    in.xruns = 0
    out.channels = 2
    out.perf = lowlat
    out.sharing = exclusive
    out.api = aaudio
    out.rate = 48000
    out.device = 27
    out.mmap = yes
    out.burst.frames = 96
    out.buffer.size.frames = 192
    out.buffer.capacity.frames = 3072
    out.xruns = 0

### Latency Report

Each test also adds specific value. For "latency". If the test fails then some values will be unavailable.

Here is a report from a good test. The '#' comments were added for this document and are not in the report.

    rms.signal = 0.81829  # Root Mean Square of the signal, if it can be detected
    rms.noise = 0.12645   # Root Mean Square of the background noise before the signal is detected
    reset.count = 2       # number of times the full duplex stream input underflowed and had to resynchronize
    result = 0            # 0 or a negative error
    result.text = OK      # text equivalent of the result
    latency.empty.frames = 476   # round trip latency if the top output buffer was empty
    latency.empty.msec =   9.92  # same but translated to milliseconds
    latency.frames = 668   # round trip latency as measured
    latency.msec =  13.92  # same but translated to milliseconds, "pro-audioo" devices should be <=20 msec
    confidence =  0.959    # quality of the latency result between 0.0 and 1.0, higher is better

Here is a report from a test that failed because the output was muted. Note the latency.msec is missing because it could not be measured.

    rms.signal = 0.00000
    rms.noise = 0.00048
    reset.count = 3
    result = -96
    result.text = ERROR_CONFIDENCE
    confidence =  0.009
        
### Glitch Report

Here is a report from a good test. The '#' comments were added for this document and are not in the report.

    tolerance = 0.123
    state = LOCKED
    unlocked.frames = 2528   # frames spent trying to lock onto the signal
    locked.frames = 384084   # frames spent locked onto a good signal with no glitches
    glitch.frames = 0        # frames spent glitching or recovering from a glitch
    reset.count = 208        # number of times the full duplex stream input underflowed and had to resynchronize
    peak.amplitude = 0.057714  # peak amplitude of the input signal, between 0.0 and 1.0
    signal.noise.ratio.db =  96.3
    time.total =     9.96 seconds  # close to your specified duration
    time.no.glitches =     9.96    # time we have been running with no glitches 
    max.time.no.glitches =     9.96 # max time with no glitches
    glitch.count = 0               # number of glitch events, actual number may be higher if close together
    
Here is a report from a test that failed because the output was muted. Note the glitch.count is missing because it could not be measured.

    state = WAITING_FOR_SIGNAL
    unlocked.frames = 0
    locked.frames = 0
    glitch.frames = 0
    reset.count = 1
    time.total =     9.95 seconds
