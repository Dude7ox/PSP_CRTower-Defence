#ifndef CRTD_AUDIO_H
#define CRTD_AUDIO_H

enum {
    AE_ARROW,
    AE_CANNON,
    AE_FROST,
    AE_SNIPER,
    AE_LASER,
    AE_HIT,
    AE_DIE,
    AE_BOSSDIE,
    AE_CASTLEHIT,
    AE_BUILD,
    AE_UPGRADE,
    AE_UPGRADE_FAIL,
    AE_SELL,
    AE_PLACE_ERROR,
    AE_WAVE_START,
    AE_WAVE_END,
    AE_CLICK,
    AE_PERK,
    AE_CURSED,
    AE_REROLL,
    AE_MOD,
    AE_THEME,
    AE_GAMEOVER,
    AE_COUNT
};

enum { WAVE_SINE, WAVE_TRI, WAVE_SQUARE, WAVE_SAW, WAVE_NOISE };

void audio_init(void);
void audio_shutdown(void);
void audio_set_muted(int muted);
int audio_muted(void);
void audio_tone(float freq, float dur, int wave, float vol, float sweep);
void audio_noise(float dur, float vol, float filter_freq);
void audio_event(int ev);

#endif
