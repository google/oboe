# How to Use OboeTester

## Test Activities

Launch OboeTester and then select one of the test Activities.

### Test Output

Test opening, starting, stopping and closing a stream.
Various stream parameters can be selected before opening.
While the stream is playing, the frame counts and stream state are displayed.
The latency is estimated based on the timestamp information.

### Test Input

Test input streams. Displays current volume as VU bars.

### Tap to Tone Latency

Measure touch screen latency plus audio output latency.
Open, Start, then tap on the screen with your fingertip.
The app will listen for the sound of your fingernail tapping the screen
and the resulting beep, and then measure the time between them.
If you use headphones then you can eliminate the latency caused by speaker protection.
If you use USB-MIDI input then you can eliminate the latency due to the touch screen, which is around 15-30 msec.
MIDI latency is generally under 1 msec.

### Record and Playback

Record several seconds of audio and play it back.
The Share button lets you share the recorded WAV file.
You can then examine the WAV file using a program like Audacity.

### Echo Input to Output

This test copies input to output and adds up to 3 seconds of delay.
This can be used to simulate high latency on a phone that supports low latency.
Try putting the phone to your ear with the added delay at 0 and try talking.
Then set it to about 100 msec and try talking on the phone. Notice how the echo can be very confusing.

### Round Trip Latency

This test works with either a loopback adapter or through speakers.
Latency through the speakers will probably be higher.
It measures the input and output latency combined.
Set the volume somewhere in the middle.

### Glitch Test

This test works best with a loopback adapter. But it can also work in a quiet room.
It plays a sine wave and then tries to record and lock onto that sine wave.
If the actual input does not match the expected sine wave value then it is counted as a glitch.
The test will display the maximum time that it ran without seeing a glitch.
A "reset" occurs if there is not enough input data, which causes a gap in the signal.
Some resets are common at the beginnng.

### Auto Glitch Test

Measure glitches for various combinations of input and output settings.
Change the test duration to a high value and let it run for a while.
If you get glitches in one configuration then you can investigate using the Manual Glitch Test.
The Share button will let you email the report to yourself.
