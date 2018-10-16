# Android Device Latency

DRAFT - ALL INFORMATION SHOULD BE CONSIDERED INACCURATE

We've tested a number of Android devices for their round-trip audio latency. The following is a list of devices which either conform to the [Android Professional Audio latency specification](https://source.android.com/compatibility/android-cdd#5_10_professional_audio) (less than 20ms), or are close to it (under 50ms).  

If you would like a device tested please [add a comment to this issue](https://github.com/google/oboe/issues/248).

## [Android Pro Audio](https://source.android.com/compatibility/android-cdd#5_10_professional_audio) compliant (<20ms)
These devices have less than 20ms round-trip audio latency.

| Manufacturer | Model | Android version | Build number | Round-trip latency (ms) | Notes |
|---|---|---|---|---|
| HTC / Google | Nexus 9 | 7.0.0 Nougat | NRD91R | 15.22 |   |
| LG / Google | Nexus 5X | 8.1.0 Oreo MR1 | OPM1.170918.001 | 17.72  |   |
| Huawei / Google | Nexus 6P | 8.0.0 Oreo  | OPR5.170623.007  | 16.83  | |
| Google | Pixel XL | 9.0.0 Pie | PPR2.181005.003 | 13.52  |   |
| Google | Pixel 2 | 8.1.0 Oreo MR1 | OPM1.171019.011 | 18.08  | |
| Samsung | Galaxy Note 9 | 8.1.0 Oreo MR1 | M1AJQ.N960FXXUOARZM | 19.33 | |


## Nearly compliant (<50ms)
These devices have less than 50ms round-trip audio latency.

| Manufacturer | Model | Android version | Build number | Round-trip latency (ms) | Notes |
|---|---|---|---|---|
| Samsung | Galaxy S9 (SM-G960F) | 8.0.0 Oreo | G960FXXS2BRI1 | 20.95 | SLES was used ([see bug](https://github.com/gkasten/drrickorang/issues/28))|

## Test method
The following method was used to measure the round-trip audio latency:

* Insert [loopback adapter](https://source.android.com/devices/audio/latency/loopback) into 3.5mm jack (if device has one) or into the USB headphone adapter supplied with the device
* Run [LoopbackApp](https://play.google.com/store/apps/details?id=org.drrickorang.loopback)
* If device is running Android 8.0 (Oreo) or above go to Settings->Audio Thread Type change to "native (AAudio)"
* Tap "Calibrate sound level now" to set the volume automatically
* Tap the "Round-trip latency test"
* Run test 6 times taking the average
