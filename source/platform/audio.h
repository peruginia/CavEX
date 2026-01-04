#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

enum SoundEffect {
    SFX_BREAK,
    SFX_PLACE,
    SFX_WALK,
    SFX_COUNT
};

void audio_init();
void audio_stop();
void audio_load_resources();
void audio_play_sfx(enum SoundEffect sfx);

// Avvia lo streaming di un file musicale (es. "music/calm1.raw")
void audio_play_music(const char* path);

// Da chiamare nel ciclo principale del gioco (main loop)
void audio_update_streams();

#endif