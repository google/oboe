# Projects using Oboe or AAudio
Based on Google Play data there are over 200 apps using Oboe, with over 4 billion installs. Here is a list of those who have given permission to be listed publicly. 

Want your project added? [Add a comment to issue #214](https://github.com/google/oboe/issues/214) with 
your project name and Play Store URL. 

| Name | Description | Publisher | Notes |
|:--|:--|:--|:--|
| [4Beats](https://play.google.com/store/apps/details?id=com.fourbeats) | Easy song creation tool | Code sign | 44100 Hz so no MMAP |
| [Audio Evolution Mobile Studio](https://play.google.com/store/apps/details?id=com.extreamsd.aemobile) | Digital Audio Workstation app | eXtream Software Development | Oboe implemented in version 4.9.8.1 |
| [AudioLab - Audio Editor Recorder & Ringtone Maker](https://play.google.com/store/apps/details?id=com.hitrolab.audioeditor) | THE ONLY AUDIO EDITOR APP YOU WILL EVER NEED | HitroLab | Using Oboe in AudioLab Karaoke offline feature to record audio with the lowest latency |
| BeatScratch [Free](https://play.google.com/store/apps/details?id=com.jonlatane.beatpad.free), [Pro](https://play.google.com/store/apps/details?id=com.jonlatane.beatpad) | 3-instrument MIDI controller, sequencer, and 5-part musical notepad | Jon Latané | Using FluidSynth's Oboe Driver |
| [Best Piano](https://play.google.com/store/apps/details?id=com.netigen.piano) | Piano tutor app  | Netigen | Stream is opened with 44100 Hz so it will not get an MMAP stream on Pixel |
| [CSound for Android](https://play.google.com/store/apps/details?id=com.csounds.Csound6) | Audio synthesis app, using CSound framework | Irreducible Productions | [Oboe implementation source code](https://github.com/gogins/csound-extended/blob/develop/CsoundForAndroid/CsoundAndroid/jni/csound_oboe.hpp) |
| [Faust](https://github.com/grame-cncm/faust) | DSP language, exports to Android | [Grame](https://www.grame.fr/) | [Oboe implementation source code](https://github.com/grame-cncm/faust/blob/master-dev/architecture/faust/audio/oboe-dsp.h) |
| [FluidSynth](https://github.com/FluidSynth/fluidsynth) | MIDI synthesizer based on SoundFont 2 | [fluidsynth.org](http://www.fluidsynth.org) | drivers for Oboe and OpenSL ES |
| [Grainstorm](https://play.google.com/store/apps/details?id=me.rocks.grainstorm) | Granular synthesizer app | The Secret Laboratory | |
| [G-Stomper apps](https://play.google.com/store/apps/dev?id=5200192441928542082) | Mobile music production software | planet-h.com | Uses AAudio if you enable it in Settings (Setup->AUD->Audio System->AAudio). |
| [JUCE](https://juce.com/) | Middleware framework | [ROLI](https://www.roli.com) | Oboe support enabled as experimental feature in Projucer |
| [ktnes](https://github.com/felipecsl/ktnes) | A NES emulator implemented in Kotlin using multiplatform support and Kotlin/Native. | Felipe Lima | | 
| [Les Talens Lyriques apps](https://play.google.com/store/apps/developer?id=Les+Talens+Lyriques) | Music education apps | Les Talens Lyriques |  Stream opened with 44100 Hz so it will not get an MMAP stream on Pixel |
| [libGDX Oboe](https://github.com/barsoosayque/libgdx-oboe) | Middleware | barsoosayque |  Libgdx audio replacement for Android built on top of Oboe. |
| [Koala Sampler](https://play.google.com/store/apps/details?id=com.elf.koalasampler) | Koala is the ultimate pocket-sized sampler. Record anything with your phone's mic instantly. Use Koala to create beats with those samples, add effects and create a track! | elf audio |
| [Mini Piano Lite](https://play.google.com/store/apps/details?id=umito.android.minipiano) | Piano keyboard | Umito | Using AAudio  |
| [Mini Tunes](https://play.google.com/store/apps/details?id=com.minitunes) | Microtonal Synthesizer for Android | Fade Apps | Oboe implemented in version 2.0 | 
| [Music Speed Changer](https://play.google.com/store/apps/details?id=com.smp.musicspeed) | Play song files while changing the pitch and tempo. | Single Minded Productions |  | 
| [Musician's Aide](https://play.google.com/store/apps/details?id=com.musiciansAide.app) | Musician's Aide strives to simplify your practice sessions and double your efficiency. | Astar App Foundry |  | 
| [n-Track Studio](https://play.google.com/store/apps/details?id=com.ntrack.studio.demo) | Mobile audio workstation | n-Track Software | Settings->Select AAudio for input and/or output->OK |
| [Pinoy Piano](https://play.google.com/store/apps/details?id=kheldiente.midien.pinoypiano) | Piano rhythm game. Play and Listen to your favorite Filipino songs. | Michael Diente | |
| [Pocket Shruti Box: Carnatic Tambura](https://play.google.com/store/apps/details?id=org.kuyil.shrutibox) | High quality Tambura accompaniment for carnatic musicians and students | [Kuyil Carnatic Apps](https://kuyil.org/) |
| [Quieter Calm](https://play.google.com/store/apps/details?id=quieter.app.calm) | Calming soundscapes and visuals | [Quieter](https://quieter.net/calm/) |
| [RemixLive](https://www.mixvibes.com/remixlive-remix-app/) | Loop sequences, create beats | [Mixvibes](https://www.mixvibes.com/) | using AAudio |
| [AudioSerial](https://davidawehr.com/blog/audioserial/) | Serial communication via audio | David Wehr | blog and source code |
| [Sound Amplifier](https://play.google.com/store/apps/details?id=com.google.android.accessibility.soundamplifier) | Hearing assistant | Google | Enable through Settings > Accessability |
| [SoundCloud](https://play.google.com/store/apps/details?id=com.soundcloud.android) | Music streaming app | SoundCloud | They [wrote an article](https://developers.soundcloud.com/blog/soundcloud-is-playing-the-oboe) about the integration |
| [Shruti Carnatic Tuner](https://play.google.com/store/apps/details?id=org.kuyil.shruti) | Shruti helps to tune your instruments or voice to accurate Carnatic swarams. | Kuyil |  |
| Volcano Mobile apps | MIDI synthesizer apps: [FluidSynth](https://play.google.com/store/apps/details?id=net.volcanomobile.fluidsynthmidi), [OPL3 MIDI Synth FM](https://play.google.com/store/apps/details?id=net.volcanomobile.opl3midisynth), [MIDI Sequencer](https://play.google.com/store/apps/details?id=net.volcanomobile.midisequencer) | Volcano Mobile |  MIDI Sequencer app is very handy for testing. |

