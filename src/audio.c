#include <pspkernel.h>
#include <pspaudio.h>
#include <pspthreadman.h>
#include <math.h>
#include <string.h>

#include "audio.h"

#define SR 44100
#define BLOCK 512
#define NVOICES 12

typedef struct {
    int active;
    int wave;
    float phase;
    float f0, f1;
    float t, dur;
    float vol;
    float lp;
    float lp_a;
} Voice;

static Voice voices[NVOICES];
static int next_voice;
static int audio_ch = -1;
static int audio_thid = -1;
static volatile int audio_running;
static volatile int muted;
static short mixbuf[BLOCK * 2] __attribute__((aligned(16)));
static unsigned int rng_state = 0x9e3779b9u;

static float frand(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return ((int)(rng_state >> 16) - 32768) / 32768.0f;
}

static float voice_sample(Voice *v) {
    if (!v->active) return 0.0f;
    float u = v->t / v->dur;
    if (u >= 1.0f) { v->active = 0; return 0.0f; }

    float f = v->f0 + (v->f1 - v->f0) * u;
    if (f < 10.0f) f = 10.0f;
    if (f > 12000.0f) f = 12000.0f;
    v->phase += f / (float)SR;
    if (v->phase >= 1.0f) v->phase -= floorf(v->phase);

    float s = 0.0f;
    switch (v->wave) {
        case WAVE_SINE:
            s = sinf(v->phase * 6.2831853f);
            break;
        case WAVE_TRI:
            s = 1.0f - 4.0f * fabsf(v->phase - 0.5f);
            break;
        case WAVE_SQUARE:
            s = (v->phase < 0.5f) ? 1.0f : -1.0f;
            break;
        case WAVE_SAW:
            s = 2.0f * v->phase - 1.0f;
            break;
        case WAVE_NOISE:
            v->lp += v->lp_a * (frand() - v->lp);
            s = v->lp * 1.8f;
            break;
    }

    float attack = 0.02f;
    if (attack > v->dur * 0.3f) attack = v->dur * 0.3f;
    float env;
    if (v->t < attack) env = v->t / attack;
    else env = expf(-5.5f * (v->t - attack) / (v->dur - attack));
    if (env < 0.0f) env = 0.0f;
    v->t += 1.0f / (float)SR;
    return s * env * v->vol;
}

static int mixer_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    while (audio_running) {
        for (int i = 0; i < BLOCK; i++) {
            float acc = 0.0f;
            if (!muted) {
                for (int v = 0; v < NVOICES; v++) acc += voice_sample(&voices[v]);
            }
            /* 3x louder than the original mix; hard-limit below. */
            acc *= 2.7f;
            if (acc > 1.0f) acc = 1.0f;
            if (acc < -1.0f) acc = -1.0f;
            short s = (short)(acc * 32000.0f);
            mixbuf[i * 2] = s;
            mixbuf[i * 2 + 1] = s;
        }
        sceKernelDcacheWritebackRange(mixbuf, sizeof(mixbuf));
        sceAudioOutputPannedBlocking(audio_ch, PSP_AUDIO_VOLUME_MAX,
                                     PSP_AUDIO_VOLUME_MAX, mixbuf);
    }
    return 0;
}

void audio_init(void) {
    memset(voices, 0, sizeof(voices));
    audio_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BLOCK,
                                 PSP_AUDIO_FORMAT_STEREO);
    if (audio_ch < 0) return;
    audio_running = 1;
    audio_thid = sceKernelCreateThread("tdmix", mixer_thread, 0x12, 0x2000, 0, NULL);
    if (audio_thid >= 0) sceKernelStartThread(audio_thid, 0, NULL);
}

void audio_shutdown(void) {
    if (audio_ch < 0) return;
    audio_running = 0;
    if (audio_thid >= 0) {
        sceKernelWaitThreadEnd(audio_thid, NULL);
        sceKernelDeleteThread(audio_thid);
        audio_thid = -1;
    }
    sceAudioChRelease(audio_ch);
    audio_ch = -1;
}

void audio_set_muted(int m) { muted = m ? 1 : 0; }
int audio_muted(void) { return muted; }

void audio_tone(float freq, float dur, int wave, float vol, float sweep) {
    if (audio_ch < 0 || muted) return;
    int idx = -1;
    for (int i = 0; i < NVOICES; i++) {
        int k = (next_voice + i) % NVOICES;
        if (!voices[k].active) { idx = k; break; }
    }
    if (idx < 0) idx = next_voice;
    next_voice = (idx + 1) % NVOICES;

    Voice *v = &voices[idx];
    v->active = 1;
    v->wave = wave;
    v->phase = 0.0f;
    v->f0 = freq;
    v->f1 = (sweep > 0.0f) ? sweep : freq;
    v->t = 0.0f;
    v->dur = dur;
    v->vol = vol;
    v->lp = 0.0f;
    v->lp_a = 0.4f;
}

void audio_noise(float dur, float vol, float filter_freq) {
    if (audio_ch < 0 || muted) return;
    int idx = -1;
    for (int i = 0; i < NVOICES; i++) {
        int k = (next_voice + i) % NVOICES;
        if (!voices[k].active) { idx = k; break; }
    }
    if (idx < 0) idx = next_voice;
    next_voice = (idx + 1) % NVOICES;

    Voice *v = &voices[idx];
    v->active = 1;
    v->wave = WAVE_NOISE;
    v->phase = 0.0f;
    v->f0 = 1000.0f;
    v->f1 = 1000.0f;
    v->t = 0.0f;
    v->dur = dur;
    v->vol = vol;
    v->lp = 0.0f;
    float a = 1.0f - expf(-6.2831853f * filter_freq / (float)SR);
    if (a < 0.002f) a = 0.002f;
    if (a > 0.95f) a = 0.95f;
    v->lp_a = a;
}

void audio_event(int ev) {
    switch (ev) {
        case AE_ARROW:
            audio_tone(420, 0.07f, WAVE_TRI, 0.06f, 280);
            break;
        case AE_CANNON:
            audio_tone(90, 0.18f, WAVE_SINE, 0.14f, 45);
            audio_noise(0.12f, 0.14f, 250);
            break;
        case AE_FROST:
            /* Softer and lower than the old 760->900 Hz sine: many frost
               towers firing together clipped the master bus. */
            audio_tone(600, 0.08f, WAVE_SINE, 0.03f, 720);
            break;
        case AE_SNIPER:
            audio_tone(220, 0.18f, WAVE_TRI, 0.12f, 110);
            break;
        case AE_LASER:
            audio_tone(380, 0.22f, WAVE_TRI, 0.09f, 180);
            audio_tone(190, 0.22f, WAVE_SINE, 0.06f, 80);
            break;
        case AE_HIT:
            audio_tone(140, 0.035f, WAVE_SINE, 0.05f, 110);
            break;
        case AE_DIE:
            audio_tone(280, 0.12f, WAVE_TRI, 0.09f, 90);
            audio_noise(0.08f, 0.07f, 350);
            break;
        case AE_BOSSDIE:
            audio_tone(110, 0.40f, WAVE_SINE, 0.16f, 40);
            audio_noise(0.35f, 0.12f, 300);
            break;
        case AE_CASTLEHIT:
            audio_tone(55, 0.30f, WAVE_SINE, 0.18f, 35);
            audio_noise(0.18f, 0.12f, 200);
            break;
        case AE_BUILD:
            audio_tone(280, 0.09f, WAVE_TRI, 0.10f, 0);
            audio_tone(420, 0.12f, WAVE_TRI, 0.09f, 0);
            break;
        case AE_UPGRADE:
            audio_tone(300, 0.10f, WAVE_TRI, 0.10f, 0);
            audio_tone(400, 0.10f, WAVE_TRI, 0.10f, 0);
            audio_tone(530, 0.16f, WAVE_TRI, 0.11f, 0);
            break;
        case AE_UPGRADE_FAIL:
            audio_tone(150, 0.15f, WAVE_SQUARE, 0.08f, 120);
            break;
        case AE_SELL:
            audio_tone(400, 0.10f, WAVE_TRI, 0.10f, 0);
            audio_tone(240, 0.16f, WAVE_TRI, 0.10f, 0);
            break;
        case AE_PLACE_ERROR:
            audio_tone(90, 0.18f, WAVE_TRI, 0.14f, 60);
            break;
        case AE_WAVE_START:
            audio_tone(180, 0.18f, WAVE_TRI, 0.12f, 0);
            audio_tone(260, 0.18f, WAVE_TRI, 0.12f, 0);
            audio_tone(340, 0.28f, WAVE_TRI, 0.14f, 0);
            break;
        case AE_WAVE_END:
            audio_tone(340, 0.14f, WAVE_TRI, 0.12f, 0);
            audio_tone(420, 0.14f, WAVE_TRI, 0.12f, 0);
            audio_tone(560, 0.22f, WAVE_TRI, 0.13f, 0);
            break;
        case AE_CLICK:
            audio_tone(520, 0.03f, WAVE_SINE, 0.05f, 0);
            break;
        case AE_PERK:
            audio_tone(400, 0.12f, WAVE_TRI, 0.14f, 0);
            audio_tone(530, 0.12f, WAVE_TRI, 0.14f, 0);
            audio_tone(700, 0.20f, WAVE_TRI, 0.16f, 0);
            audio_tone(950, 0.30f, WAVE_SINE, 0.14f, 0);
            break;
        case AE_CURSED:
            audio_tone(140, 0.40f, WAVE_SAW, 0.14f, 70);
            audio_tone(110, 0.50f, WAVE_SAW, 0.12f, 55);
            audio_tone(80, 0.70f, WAVE_SINE, 0.14f, 40);
            break;
        case AE_REROLL:
            audio_tone(600, 0.08f, WAVE_TRI, 0.10f, 400);
            audio_tone(500, 0.08f, WAVE_TRI, 0.10f, 350);
            audio_tone(700, 0.12f, WAVE_TRI, 0.12f, 500);
            break;
        case AE_MOD:
            audio_tone(220, 0.30f, WAVE_SAW, 0.12f, 110);
            audio_tone(180, 0.40f, WAVE_SAW, 0.10f, 90);
            break;
        case AE_THEME:
            audio_tone(660, 0.08f, WAVE_TRI, 0.10f, 0);
            audio_tone(880, 0.12f, WAVE_TRI, 0.10f, 0);
            break;
        case AE_GAMEOVER:
            audio_tone(180, 0.50f, WAVE_SINE, 0.18f, 60);
            audio_tone(140, 0.60f, WAVE_SINE, 0.18f, 50);
            audio_tone(100, 1.00f, WAVE_SINE, 0.18f, 35);
            break;
    }
}
