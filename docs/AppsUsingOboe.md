# Projects using Oboe or AAudio

| Name | Description | Publisher | Notes |
|:--|:--|:--|:--|
| [Best Piano](https://play.google.com/store/apps/details?id=com.netigen.piano) | Piano tutor app  | Netigen | Stream is opened with 44100 Hz so it will not get an MMAP stream on Pixel |
| [CSound for Android](https://play.google.com/store/apps/details?id=com.csounds.Csound6) | Audio synthesis app, using CSound framework | Irreducible Productions | [Oboe implementation source code](https://github.com/gogins/csound-extended/blob/develop/CsoundForAndroid/CsoundAndroid/jni/csound_oboe.hpp) |
| [Grainstorm](https://play.google.com/store/apps/details?id=me.rocks.grainstorm) | Granular synthesizer app | The Secret Laboratory | |
| [G-Stomper apps](https://play.google.com/store/apps/dev?id=5200192441928542082) | Mobile music production software | planet-h.com | Uses AAudio if you enable it in Settings (Setup->AUD->Audio System->AAudio). |
| [ktnes](https://github.com/felipecsl/ktnes) | A NES emulator implemented in Kotlin using multiplatform support and Kotlin/Native. | Felipe Lima | | 
| [Les Talens Lyriques apps](https://play.google.com/store/apps/developer?id=Les+Talens+Lyriques) | Music education apps | Les Talens Lyriques |  Stream opened with 44100 Hz so it will not get an MMAP stream on Pixel |
| [JUCE](https://juce.com/) | Middleware framework | [ROLI](https://www.roli.com) | Oboe support enabled as experimental feature in Projucer |
| [Music Speed Changer](https://play.google.com/store/apps/details?id=com.smp.musicspeed) | Play song files while changing the pitch and tempo. | Single Minded Productions |  | 
| [n-Track Studio](https://play.google.com/store/apps/details?id=com.ntrack.studio.demo) | Mobile audio workstation | n-Track Software | Settings->Select AAudio for input and/or output->OK |
| [Serial communication via audio](https://davidawehr.com/blog/audioserial/) | | David Wehr |  |
| Volcano Mobile apps | MIDI synthesizer apps: [FluidSynth](https://play.google.com/store/apps/details?id=net.volcanomobile.fluidsynthmidi), [OPL3 MIDI Synth FM](https://play.google.com/store/apps/details?id=net.volcanomobile.opl3midisynth), [MIDI Sequencer](https://play.google.com/store/apps/details?id=net.volcanomobile.midisequencer) | Volcano Mobile |  MIDI Sequencer app is very handy for testing. |

Want your project added? [Add a comment to issue #214](https://github.com/google/oboe/issues/214) with 
your project name and Play Store URL. 
