#include "../include/drivers.h"
#include "../include/hal.h"

#define PORT_PIT_CMD   0x43
#define PORT_PIT_CH2   0x42
#define PORT_SPEAKER   0x61

#define SPEAKER_GATE   0x01
#define SPEAKER_ENABLE 0x02
#define SPEAKER_FREQ   1193182U

#define REST    0U
#define NOTE_C4 262U
#define NOTE_D4 294U
#define NOTE_E4 330U
#define NOTE_F4 349U
#define NOTE_G4 392U
#define NOTE_A4 440U
#define NOTE_B4 494U
#define NOTE_C5 523U
#define NOTE_D5 587U
#define NOTE_E5 659U
#define NOTE_F5 698U
#define NOTE_G5 784U
#define NOTE_A5 880U
#define NOTE_C6 1047U

static volatile uint8_t  g_speaker_state = 0;
static volatile bool     g_sound_enabled = true;
static volatile bool     g_playing = false;
static volatile bool     g_repeat = false;
static const SOUND_NOTE* volatile g_song = NULL;
static volatile uint16_t g_song_len = 0;
static volatile uint16_t g_song_pos = 0;
static volatile uint16_t g_ticks_left = 0;

static void speaker_off(void) {
    g_speaker_state = (uint8_t)(g_speaker_state & (uint8_t)~(SPEAKER_GATE | SPEAKER_ENABLE));
    HalOutPort8(PORT_SPEAKER, g_speaker_state);
}

static void speaker_tone(uint16_t hertz) {
    if (!g_sound_enabled || hertz < 20) {
        speaker_off();
        return;
    }
    uint16_t divisor = (uint16_t)(SPEAKER_FREQ / hertz);
    HalOutPort8(PORT_PIT_CMD, 0xB6);
    HalOutPort8(PORT_PIT_CH2, (uint8_t)(divisor & 0xFF));
    HalOutPort8(PORT_PIT_CH2, (uint8_t)(divisor >> 8));

    g_speaker_state = (uint8_t)(HalInPort8(PORT_SPEAKER) & (uint8_t)~(SPEAKER_GATE | SPEAKER_ENABLE));
    g_speaker_state = (uint8_t)(g_speaker_state | SPEAKER_GATE | SPEAKER_ENABLE);
    HalOutPort8(PORT_SPEAKER, g_speaker_state);
}

void DrvSoundInit(void) {
    g_speaker_state = (uint8_t)(HalInPort8(PORT_SPEAKER) & (uint8_t)~(SPEAKER_GATE | SPEAKER_ENABLE));
    HalOutPort8(PORT_SPEAKER, g_speaker_state);
    g_song = NULL;
    g_song_len = 0;
    g_song_pos = 0;
    g_ticks_left = 0;
    g_repeat = false;
    g_playing = false;
    HalPrintf("[DRV] PC Speaker Sound initialized (PIT Ch2 / Port 0x61)\n");
}

void DrvSoundPlay(const SOUND_NOTE* notes, uint16_t count, bool repeat) {
    if (!notes || count == 0 || !g_sound_enabled) {
        DrvSoundStop();
        return;
    }
    g_song = notes;
    g_song_len = count;
    g_song_pos = 0;
    g_repeat = repeat;
    g_playing = true;
    g_ticks_left = notes[0].ticks;
    speaker_tone(notes[0].hertz);
}

void DrvSoundStop(void) {
    g_playing = false;
    g_song = NULL;
    g_song_len = 0;
    g_song_pos = 0;
    g_ticks_left = 0;
    speaker_off();
}

bool DrvSoundIsPlaying(void) {
    return g_playing;
}

void DrvSoundSetEnabled(bool enabled) {
    g_sound_enabled = enabled;
    if (!enabled) {
        DrvSoundStop();
    }
}

bool DrvSoundIsEnabled(void) {
    return g_sound_enabled;
}

void DrvSoundTick(void) {
    if (!g_playing || !g_song) return;
    if (g_ticks_left > 1) {
        g_ticks_left--;
        return;
    }
    g_song_pos++;
    if (g_song_pos >= g_song_len) {
        if (!g_repeat) {
            DrvSoundStop();
            return;
        }
        g_song_pos = 0;
    }
    speaker_tone(g_song[g_song_pos].hertz);
    g_ticks_left = g_song[g_song_pos].ticks;
}

static const SOUND_NOTE g_startup_chime[] = {
    { NOTE_G4, 4 }, { NOTE_C5, 4 }, { NOTE_E5, 4 }, { NOTE_G5, 6 },
    { NOTE_E5, 3 }, { NOTE_G5, 3 }, { NOTE_C6, 16 }
};

static const SOUND_NOTE g_march[] = {
    { NOTE_G4, 6 }, { NOTE_G4, 3 }, { NOTE_G4, 6 }, { NOTE_D4, 4 },
    { NOTE_G4, 8 }, { NOTE_B4, 8 }, { NOTE_C5, 6 }, { NOTE_B4, 4 },
    { NOTE_A4, 8 }, { NOTE_G4, 8 }, { NOTE_E4, 6 }, { NOTE_G4, 10 },
    { REST, 4 }
};

static const SOUND_NOTE g_waltz[] = {
    { NOTE_C5, 9 }, { NOTE_E5, 3 }, { NOTE_G5, 6 },
    { NOTE_A5, 9 }, { NOTE_G5, 3 }, { NOTE_E5, 6 },
    { NOTE_F5, 9 }, { NOTE_D5, 3 }, { NOTE_F5, 6 },
    { NOTE_E5, 12 }, { REST, 3 }
};

static const SOUND_NOTE g_bleep[] = {
    { NOTE_A5, 3 }
};

static const SOUND_NOTE g_two_tone[] = {
    { NOTE_E5, 4 }, { NOTE_A4, 6 }
};

static const SOUND_NOTE g_nom_gulp[] = {
    { NOTE_A4, 3 }, { NOTE_E4, 4 }
};

static const SOUND_NOTE g_burp_rumble[] = {
    { NOTE_G4, 7 }, { NOTE_E4, 7 }, { NOTE_C4, 13 }, { REST, 2 },
    { NOTE_C4, 6 }
};

void DrvSoundPlayStartup(void) {
    DrvSoundPlay(g_startup_chime, (uint16_t)(sizeof(g_startup_chime) / sizeof(g_startup_chime[0])), false);
}

void DrvSoundPlayMarch(void) {
    DrvSoundPlay(g_march, (uint16_t)(sizeof(g_march) / sizeof(g_march[0])), false);
}

void DrvSoundPlayWaltz(void) {
    DrvSoundPlay(g_waltz, (uint16_t)(sizeof(g_waltz) / sizeof(g_waltz[0])), false);
}

void DrvSoundPlayNom(void) {
    DrvSoundPlay(g_nom_gulp, (uint16_t)(sizeof(g_nom_gulp) / sizeof(g_nom_gulp[0])), false);
}

void DrvSoundPlayBurp(void) {
    DrvSoundPlay(g_burp_rumble, (uint16_t)(sizeof(g_burp_rumble) / sizeof(g_burp_rumble[0])), false);
}

void DrvSoundBeep(void) {
    DrvSoundPlay(g_bleep, 1, false);
}

void DrvSoundAlert(void) {
    DrvSoundPlay(g_two_tone, 2, false);
}
