#include "audio.h"

#ifdef _WIN32

#include <windows.h>
#include <mmsystem.h>
#include <vector>
#include <cmath>
#include <cstdlib>

namespace
{
const int   RATE       = 22050;
const int   NBUF       = 4;
const int   BUFSAMPLES = 1024;
const int   NVOICES    = 24;
const float PI         = 3.14159265f;

struct Voice
{
    const std::vector<short>* data;
    size_t pos;
    float  vol;
    bool   loop;
    bool   active;
};

std::vector<short> sfxData[SFX_COUNT];
std::vector<short> musicData;
std::vector<short> ambData;

Voice            voices[NVOICES];
CRITICAL_SECTION cs;
HWAVEOUT         hwo = NULL;
WAVEHDR          hdr[NBUF];
short            bufs[NBUF][BUFSAMPLES];
HANDLE           hThread = NULL;
volatile bool    running = false;
volatile bool    soundOn = true;
bool             inited  = false;

// ------------------------------------------------------------------
// Synthesis helpers
// ------------------------------------------------------------------
float rnd() { return (std::rand() / (float)RAND_MAX) * 2.0f - 1.0f; }

short toShort(float v)
{
    if (v > 1.0f)  v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return (short)(v * 30000.0f);
}

std::vector<short> fromFloat(const std::vector<float>& f, float target)
{
    float peak = 0.0001f;
    for (size_t i = 0; i < f.size(); i++)
        if (std::fabs(f[i]) > peak) peak = std::fabs(f[i]);

    std::vector<short> out(f.size());
    for (size_t i = 0; i < f.size(); i++)
        out[i] = toShort(f[i] / peak * target);
    return out;
}

void makeBoom()
{
    int n = (int)(1.8f * RATE);
    std::vector<float> f(n);
    float lp = 0, phase = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        lp += (rnd() - lp) * 0.07f;
        phase += 2 * PI * (50.0f + 90.0f * std::exp(-t * 9.0f)) / RATE;
        f[i] = lp * 3.0f * std::exp(-t * 2.2f) + 0.9f * std::sin(phase) * std::exp(-t * 4.0f);
    }
    sfxData[SFX_BOOM] = fromFloat(f, 0.95f);
}

void makeCrackle()
{
    int n = (int)(1.1f * RATE);
    std::vector<float> f(n);
    float y = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        float prob = 0.006f * std::exp(-t * 2.2f);
        if ((std::rand() / (float)RAND_MAX) < prob) y = rnd();
        f[i] = y;
        y *= 0.72f;
    }
    sfxData[SFX_CRACKLE] = fromFloat(f, 0.7f);
}

void makeWhistle()
{
    int n = (int)(0.8f * RATE);
    std::vector<float> f(n);
    float phase = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        float u = t / 0.8f;
        phase += 2 * PI * (600.0f + 1500.0f * std::pow(u, 1.5f)) / RATE;
        float env = std::sqrt(std::sin(PI * u));
        f[i] = std::sin(phase) * env * 0.3f + rnd() * 0.02f * env;
    }
    sfxData[SFX_WHISTLE] = fromFloat(f, 0.5f);
}

void makeSplash()
{
    int n = (int)(0.4f * RATE);
    std::vector<float> f(n);
    float prev = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        float nz = rnd();
        float hp = nz - prev;
        prev = nz;
        float att = t < 0.01f ? t / 0.01f : 1.0f;
        f[i] = hp * std::exp(-t * 9.0f) * att;
    }
    sfxData[SFX_SPLASH] = fromFloat(f, 0.6f);
}

void makeDing()
{
    int n = (int)(0.9f * RATE);
    std::vector<float> f(n);
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        f[i] = (std::sin(2 * PI * 1318.0f * t)
              + 0.5f  * std::sin(2 * PI * 3637.0f * t)
              + 0.25f * std::sin(2 * PI * 7116.0f * t)) * std::exp(-t * 6.0f);
    }
    sfxData[SFX_DING] = fromFloat(f, 0.55f);
}

void makePop()
{
    int n = (int)(0.18f * RATE);
    std::vector<float> f(n);
    float phase = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        phase += 2 * PI * (120.0f + 700.0f * std::exp(-t * 18.0f)) / RATE;
        f[i] = std::sin(phase) * std::exp(-t * 22.0f) + rnd() * 0.12f * std::exp(-t * 60.0f);
    }
    sfxData[SFX_POP] = fromFloat(f, 0.7f);
}

void makeThud()
{
    int n = (int)(0.25f * RATE);
    std::vector<float> f(n);
    float phase = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        phase += 2 * PI * (60.0f + 170.0f * std::exp(-t * 10.0f)) / RATE;
        f[i] = std::sin(phase) * std::exp(-t * 14.0f);
    }
    sfxData[SFX_THUD] = fromFloat(f, 0.7f);
}

void addBell(std::vector<float>& v, float t0, float freq, float amp)
{
    size_t start = (size_t)(t0 * RATE);
    for (size_t i = 0; start + i < v.size() && i < (size_t)(1.5f * RATE); i++)
    {
        float t = i / (float)RATE;
        v[start + i] += amp * std::sin(2 * PI * freq * t) * std::exp(-t * 3.5f);
    }
}

// Festival ambience loop: crowd murmur + cicadas + wind chimes
void makeAmbience()
{
    const float LEN = 6.0f;
    int n = (int)(LEN * RATE);
    std::vector<float> f(n);
    float lp1 = 0, lp2 = 0;
    for (int i = 0; i < n; i++)
    {
        float t = i / (float)RATE;
        lp1 += (rnd() - lp1) * 0.03f;
        lp2 += (lp1 - lp2) * 0.05f;
        float murmur = lp2 * 10.0f * (0.65f + 0.35f * std::sin(2 * PI * t * 2.0f / LEN));
        float pulse  = std::sin(2 * PI * t * 8.0f);
        pulse = pulse > 0 ? pulse * pulse : 0.0f;
        float cicada = std::sin(2 * PI * 4200.0f * t) * pulse * 0.05f;
        f[i] = murmur + cicada;
    }
    addBell(f, 1.2f, 2093.0f, 0.05f);
    addBell(f, 3.9f, 2637.0f, 0.04f);
    addBell(f, 4.4f, 2349.0f, 0.03f);
    ambData = fromFloat(f, 0.6f);
}

// Plucked string (Karplus-Strong) mixed into a float buffer
void pluck(std::vector<float>& mix, float startSec, float freq, float durSec, float amp)
{
    int L = (int)(RATE / freq);
    if (L < 2) return;
    std::vector<float> line(L);
    for (int i = 0; i < L; i++) line[i] = rnd();

    size_t start = (size_t)(startSec * RATE);
    size_t count = (size_t)(durSec * RATE);
    int idx = 0;
    for (size_t i = 0; i < count && start + i < mix.size(); i++)
    {
        float out = line[idx];
        float nxt = line[(idx + 1) % L];
        line[idx] = 0.5f * (out + nxt) * 0.997f;
        idx = (idx + 1) % L;
        float fade = 1.0f;
        if (i > count - 800) fade = (count - i) / 800.0f;
        mix[start + i] += out * amp * fade;
    }
}

void taiko(std::vector<float>& mix, float startSec, float amp)
{
    size_t start = (size_t)(startSec * RATE);
    float phase = 0;
    for (int i = 0; i < (int)(0.5f * RATE) && start + i < mix.size(); i++)
    {
        float t = i / (float)RATE;
        phase += 2 * PI * (55.0f + 80.0f * std::exp(-t * 16.0f)) / RATE;
        mix[start + i] += amp * (std::sin(phase) * std::exp(-t * 8.0f)
                              + rnd() * 0.25f * std::exp(-t * 45.0f));
    }
}

void click(std::vector<float>& mix, float startSec, float amp)
{
    size_t start = (size_t)(startSec * RATE);
    float prev = 0;
    for (int i = 0; i < (int)(0.05f * RATE) && start + i < mix.size(); i++)
    {
        float t = i / (float)RATE;
        float nz = rnd();
        mix[start + i] += amp * (nz - prev) * std::exp(-t * 90.0f);
        prev = nz;
    }
}

struct Note { float beat; int n; float len; };

// Festival music: miyako-bushi scale, 92 BPM, 8 bars loop
void makeMusic()
{
    const float BPM  = 92.0f;
    const float BEAT = 60.0f / BPM;
    const int   BEATS = 32;
    int total = (int)(BEATS * BEAT * RATE);
    std::vector<float> mix(total, 0.0f);

    // E4 F4 A4 B4 C5 E5 B3 A3
    const float freq[8] = { 329.63f, 349.23f, 440.0f, 493.88f, 523.25f, 659.25f, 246.94f, 220.0f };

    const Note melody[] = {
        // phrase A
        {0,0,1},{1,2,0.5f},{1.5f,3,0.5f},{2,4,1},{3,3,0.5f},{3.5f,2,0.5f},{4,1,1},{5,0,1},{6,0,2},
        // phrase B
        {8,2,1},{9,3,1},{10,5,1.5f},{11.5f,4,0.5f},{12,3,1},{13,2,0.5f},{13.5f,1,0.5f},{14,0,2},
        // phrase A again
        {16,0,1},{17,2,0.5f},{17.5f,3,0.5f},{18,4,1},{19,3,0.5f},{19.5f,2,0.5f},{20,1,1},{21,0,1},{22,2,2},
        // phrase C
        {24,4,1},{25,3,0.5f},{25.5f,2,0.5f},{26,3,1},{27,4,1},{28,5,1.5f},{29.5f,4,0.5f},{30,3,1},{31,0,1}
    };
    for (size_t i = 0; i < sizeof(melody) / sizeof(melody[0]); i++)
        pluck(mix, melody[i].beat * BEAT, freq[melody[i].n], melody[i].len * BEAT * 1.4f, 0.55f);

    // low drone
    for (int b = 0; b < BEATS; b += 8)
        pluck(mix, b * BEAT, 164.81f, 4.0f * BEAT, 0.4f);

    // taiko and rim clicks
    for (int b = 0; b < BEATS; b += 2)
        taiko(mix, b * BEAT, (b % 4 == 0) ? 0.9f : 0.55f);
    for (int b = 0; b < BEATS; b++)
        click(mix, b * BEAT + 0.5f * BEAT, 0.12f);
    // fill at the end of phrases
    taiko(mix, 15.5f * BEAT, 0.5f);
    taiko(mix, 31.5f * BEAT, 0.5f);

    musicData = fromFloat(mix, 0.7f);
}

// ------------------------------------------------------------------
// Mixer
// ------------------------------------------------------------------
void fillBuffer(short* out)
{
    EnterCriticalSection(&cs);
    for (int i = 0; i < BUFSAMPLES; i++)
    {
        float s = 0.0f;
        for (int v = 0; v < NVOICES; v++)
        {
            Voice& vo = voices[v];
            if (!vo.active) continue;
            const std::vector<short>& d = *vo.data;
            s += d[vo.pos] * vo.vol;
            if (++vo.pos >= d.size())
            {
                if (vo.loop) vo.pos = 0;
                else vo.active = false;
            }
        }
        s /= 32768.0f;
        if (!soundOn) s = 0.0f;
        out[i] = (short)(std::tanh(s) * 30000.0f);
    }
    LeaveCriticalSection(&cs);
}

DWORD WINAPI audioThread(LPVOID)
{
    while (running)
    {
        for (int i = 0; i < NBUF; i++)
        {
            if (hdr[i].dwFlags & WHDR_DONE)
            {
                fillBuffer(bufs[i]);
                waveOutWrite(hwo, &hdr[i], sizeof(WAVEHDR));
            }
        }
        Sleep(3);
    }
    return 0;
}
} // namespace

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------
void audioInit()
{
    if (inited) return;
    InitializeCriticalSection(&cs);

    makeBoom(); makeCrackle(); makeWhistle(); makeSplash();
    makeDing(); makePop(); makeThud();
    makeAmbience();
    makeMusic();

    for (int i = 0; i < NVOICES; i++) voices[i].active = false;

    voices[0].data = &musicData; voices[0].pos = 0; voices[0].vol = 0.42f; voices[0].loop = true; voices[0].active = true;
    voices[1].data = &ambData;   voices[1].pos = 0; voices[1].vol = 0.55f; voices[1].loop = true; voices[1].active = true;

    WAVEFORMATEX wf;
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = RATE;
    wf.wBitsPerSample  = 16;
    wf.nBlockAlign     = 2;
    wf.nAvgBytesPerSec = RATE * 2;
    wf.cbSize          = 0;

    if (waveOutOpen(&hwo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        return;   // no audio device: the project still runs silently

    for (int i = 0; i < NBUF; i++)
    {
        ZeroMemory(&hdr[i], sizeof(WAVEHDR));
        hdr[i].lpData         = (LPSTR)bufs[i];
        hdr[i].dwBufferLength = BUFSAMPLES * sizeof(short);
        waveOutPrepareHeader(hwo, &hdr[i], sizeof(WAVEHDR));
        fillBuffer(bufs[i]);
        waveOutWrite(hwo, &hdr[i], sizeof(WAVEHDR));
    }

    running = true;
    hThread = CreateThread(NULL, 0, audioThread, NULL, 0, NULL);
    inited  = true;
    std::atexit(audioShutdown);
}

void audioShutdown()
{
    if (!inited) return;
    inited  = false;
    running = false;
    if (hThread)
    {
        WaitForSingleObject(hThread, 500);
        CloseHandle(hThread);
        hThread = NULL;
    }
    waveOutReset(hwo);
    for (int i = 0; i < NBUF; i++)
        waveOutUnprepareHeader(hwo, &hdr[i], sizeof(WAVEHDR));
    waveOutClose(hwo);
    DeleteCriticalSection(&cs);
}

void audioToggle() { soundOn = !soundOn; }
bool audioIsOn()   { return soundOn; }

void audioPlay(int sfx, float volume)
{
    if (!inited || !soundOn || sfx < 0 || sfx >= SFX_COUNT) return;
    EnterCriticalSection(&cs);
    for (int v = 2; v < NVOICES; v++)
    {
        if (!voices[v].active)
        {
            voices[v].data   = &sfxData[sfx];
            voices[v].pos    = 0;
            voices[v].vol    = volume;
            voices[v].loop   = false;
            voices[v].active = true;
            break;
        }
    }
    LeaveCriticalSection(&cs);
}

#else   // ---------- non-Windows: silent stubs so the code still builds ----------

void audioInit() {}
void audioShutdown() {}
void audioToggle() {}
bool audioIsOn() { return false; }
void audioPlay(int, float) {}

#endif