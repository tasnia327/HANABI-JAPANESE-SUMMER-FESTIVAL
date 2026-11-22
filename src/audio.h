#ifndef AUDIO_H
#define AUDIO_H

// HANABI audio engine (Windows, waveOut software mixer).
// Everything is synthesized in code: festival music (taiko + plucked strings),
// ambience (crowd murmur, cicadas, wind chimes) and all sound effects.
// No sound files are required.

enum SoundEffect
{
    SFX_BOOM = 0,   // firework explosion
    SFX_CRACKLE,    // firework crackle
    SFX_WHISTLE,    // rocket launch
    SFX_SPLASH,     // water splash
    SFX_DING,       // success bell
    SFX_POP,        // toy gun / ring throw
    SFX_THUD,       // ring lands on counter
    SFX_COUNT
};

void audioInit();                          // call once after glutCreateWindow()
void audioShutdown();                      // called automatically at exit
void audioToggle();                        // M key: sound on / off
bool audioIsOn();
void audioPlay(int sfx, float volume = 1.0f);

#endif