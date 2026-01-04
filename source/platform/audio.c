/*
   to create audio : 
	ffmpeg -i sci-fi-survival-dreamscape-6319.mp3 -f s16be -acodec pcm_s16be -ar 44100 -ac 1 music/calm1.raw
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <gccore.h>
#include <asndlib.h>
#include "audio.h"

#define MUSIC_VOICE       0
#define SFX_VOICE_START   1 

// Dimensione del chunk di streaming (es. 64KB ~ 0.7 secondi a 44kHz 16bit)
#define STREAM_BUF_SIZE   (64 * 1024)

// Buffer per SFX (piccoli, restano in RAM)
static void* sfx_buffers[SFX_COUNT];
static int sfx_sizes[SFX_COUNT];

// Struttura per lo streaming musicale
static struct {
    FILE* file;             // File handle aperto
    u8* buffers[2];         // Doppio buffer
    int active_buffer;      // Quale buffer sta suonando (0 o 1)
    bool playing;
    volatile bool need_load;// Flag: true se dobbiamo caricare dati dalla SD
} stream;

// Helper caricamento (per SFX statici)
static bool load_file_to_mem(const char* path, void** buffer, int* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    *buffer = memalign(32, *size); 
    if (!*buffer) { fclose(f); return false; }
    
    fread(*buffer, 1, *size, f);
    fclose(f);
    DCFlushRange(*buffer, *size);
    return true;
}

// Callback chiamato dall'interrupt audio quando un buffer finisce
static void music_callback(s32 voice) {
    if (!stream.playing) return;

    // Scambia buffer: se suonava 0, ora tocca a 1
    int next_buffer = 1 - stream.active_buffer;
    
    // Fai partire SUBITO il prossimo buffer per non avere pause
    ASND_SetVoice(MUSIC_VOICE, VOICE_MONO_16BIT, 44100, 0,
                  stream.buffers[next_buffer], STREAM_BUF_SIZE,
                  160, 160, music_callback);

    stream.active_buffer = next_buffer;
    
    // Segnala al main loop che il buffer appena finito (il vecchio active) va ricaricato
    stream.need_load = true;
}

void audio_init() {
    ASND_Init();
    ASND_Pause(0); 
    
    for(int i=0; i<SFX_COUNT; i++) sfx_buffers[i] = NULL;

    // Alloca i due buffer per lo streaming (allineati a 32 byte)
    stream.buffers[0] = memalign(32, STREAM_BUF_SIZE);
    stream.buffers[1] = memalign(32, STREAM_BUF_SIZE);
    stream.file = NULL;
    stream.playing = false;
    stream.need_load = false;
}

void audio_load_resources() {
    //load_file_to_mem("sfx/break.raw", &sfx_buffers[SFX_BREAK], &sfx_sizes[SFX_BREAK]);
    //load_file_to_mem("sfx/place.raw", &sfx_buffers[SFX_PLACE], &sfx_sizes[SFX_PLACE]);
    //load_file_to_mem("sfx/walk.raw",  &sfx_buffers[SFX_WALK],  &sfx_sizes[SFX_WALK]);
    
    // Nota: La musica non viene caricata qui, ma al momento del play
}

void audio_play_sfx(enum SoundEffect sfx) {
    if (sfx < 0 || sfx >= SFX_COUNT || !sfx_buffers[sfx]) return;
    
    int voice = ASND_GetFirstUnusedVoice();
    if (voice < SFX_VOICE_START) voice = SFX_VOICE_START; 
    
    ASND_SetVoice(voice, VOICE_MONO_16BIT, 44100, 0, 
                  sfx_buffers[sfx], sfx_sizes[sfx], 
                  255, 255, NULL);
}

void audio_play_music(const char* path) {
    // Chiudi eventuale file precedente
    if (stream.file) fclose(stream.file);
    ASND_StopVoice(MUSIC_VOICE);

    stream.file = fopen(path, "rb");
    if (!stream.file) {
        printf("Audio: Fallita apertura stream %s\n", path);
        return;
    }

    // Carica i primi dati in entrambi i buffer per partire
    fread(stream.buffers[0], 1, STREAM_BUF_SIZE, stream.file);
    fread(stream.buffers[1], 1, STREAM_BUF_SIZE, stream.file);
    DCFlushRange(stream.buffers[0], STREAM_BUF_SIZE);
    DCFlushRange(stream.buffers[1], STREAM_BUF_SIZE);

    stream.active_buffer = 0;
    stream.playing = true;
    stream.need_load = false;

    // Avvia il primo buffer
    ASND_SetVoice(MUSIC_VOICE, VOICE_MONO_16BIT, 44100, 0,
                  stream.buffers[0], STREAM_BUF_SIZE,
                  160, 160, music_callback);
}

void audio_update_streams() {
    // Se il callback ci ha segnalato che serve caricare dati...
    if (stream.playing && stream.file && stream.need_load) {
        stream.need_load = false; // Reset flag

        // Determina quale buffer è "inattivo" e va riempito
        int buffer_to_fill = 1 - stream.active_buffer;
        
        // Leggi dalla SD
        size_t bytes_read = fread(stream.buffers[buffer_to_fill], 1, STREAM_BUF_SIZE, stream.file);
        
        if (bytes_read < STREAM_BUF_SIZE) {
            // Fine del file o errore: Loop o Stop? Facciamo Loop.
            fseek(stream.file, 0, SEEK_SET);
            // Leggi il resto dall'inizio
            fread(stream.buffers[buffer_to_fill] + bytes_read, 1, STREAM_BUF_SIZE - bytes_read, stream.file);
        }

        // Importante: flush della cache per i nuovi dati letti
        DCFlushRange(stream.buffers[buffer_to_fill], STREAM_BUF_SIZE);
    }
}

void audio_stop() {
    ASND_End();
    if (stream.file) fclose(stream.file);
    if (stream.buffers[0]) free(stream.buffers[0]);
    if (stream.buffers[1]) free(stream.buffers[1]);
}
