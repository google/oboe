[Tech Notes Home](README.md)

# Glitches and Latency

Glitches are breaks in the flow of audio. they sound like pops or crackle.
Glitches are normally caused by a buffer underrun.
Keeping more data in the buffer can reduce glitching.
But the extra data adds latency. Glitches and latency are thus connected.

We have two goals:
1. Reduce latency as much as posible without causing glitching.
2. Eliminate the cause of glitches.

# Causes of Glitching

## Audio Framework
These are normally fixed by the Android framework team before a device is shipped.
1. Inaccurate MMAP timestamps causing read and write pointers to cross. Fixed by manufacturer.
2. Blocking operations in the real-time audio threads.
3. Bugs in the data path, eg. in the resampler.

## Application
1. Blocking operations, eg. network reads, in the real-time audio threads.
2. Rendering bugs.
3. Improper buffer sizing, particularly when using worker threads.
4. Content underflows, eg. when streaming audio and the network is interrupted.

## External Causes
These are the most serious causes of glitches because they are not easily
fixed by the audio team nor by the application developer.
1. Preemption by other high priority tasks.
2. Slow response by the scheduler when the app workload changes.
3. Core migration causing a loss of scheduler state, making it appear that the workload changed.

In \#2, the CPU scheduler is supposed to raise the CPU clock frequency when it sees that the application workload has increased.
But this can take about 100 msec. If you have a 4 msec buffer then it will obviously underflow.

In \#3, a core migration can make the CPU scheduler lose its knowledge of the audio task workload.
This can trigger the problem in \#2 even if the app has a steady workload.
Setting CPU affinity to a single core can prevent this. But that can get in the way of the CPU scheduler
doing its job.

The combination of \#2 and \#3 means that any low-latency audio task with a moderate to heavy workload can glitch. 
We are working with the Linux kernel teams to fix this.
Luckily the glitches can be detected by the app and the latency increased to reduce the glitches.
But the increase in latency can be significant.

# Related Bugs

#1452 - report of increased core migration on Android 12.

Search for glitches:  https://github.com/google/oboe/issues?q=is%3Aissue+glitches
