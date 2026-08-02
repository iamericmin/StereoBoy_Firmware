#include "interface.h"
#include "global_vars.h"
#include "lib/sb_util/sb_util.h"
#include "lib/buttons/buttons.h"
#include "pico/stdlib.h"
#include "stdio.h"
#include <stdint.h>

// SPI1 configuration for codec & sd card
#define PIN_SCK  30
#define PIN_MOSI 28
#define PIN_MISO 31
#define PIN_CS   32

// Codec control signals
#define PIN_DCS  33
#define PIN_DREQ 29
#define PIN_RST  27

int temp_visualizer = 6;
int exitCode = 0;
uint16_t prev_choice = 0;
uint16_t song_choice = 0;

int browse_tracks() {
    bool selected = 0;
    // read_lwbt();
    temp_visualizer = (visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        selected = 0;
        set_visualizer(6);
        printf("\r\nSong %d/%d: ", song_choice+1, track_count);
        prev_choice = song_choice;
        while (selected == 0) {
            uint8_t pressed = buttons_get_just_pressed();
            if (pressed > 0){
                if (pressed & BTN_D)      song_choice = (song_choice + 1) % track_count;
                if (pressed & BTN_U)      song_choice = (song_choice - 1 + track_count) % track_count; //added roll-over
                    // shift songs down by one and insert new song on top
                if (pressed & BTN_R)      song_choice = (song_choice + 10) % track_count;
                if (pressed & BTN_L)      song_choice = (song_choice - 10 + track_count) % track_count;
                if (pressed & BTN_B) {
                    return 100;
                }
                if (pressed & BTN_A) {
                    selected = 1;   
                    printf("Poo cum fart shit pee\n");
                }       
                sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window);
            }
            if (prev_choice != song_choice){
                printf("\r\nSong %d/%d: ", song_choice+1, track_count);
                prev_choice = song_choice;
            }
            
            sleep_ms(10);
        }
    }

    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
        printf("Error reading track metadata from cache table!\n");
    }

    get_mp3_metadata(current_track->filename, current_track);

    printf("\r\n\rNOW PLAYING:\r\n");
    printf("  Title : %s\r\n", current_track->title);
    printf("  Artist: %s\r\n", current_track->artist);
    printf("  Album : %s\r\n", current_track->album);
    printf("  Bitrate : %d Kbps\r\n", current_track->bitrate);
    printf("  Sample rate : %d Hz\r\n", current_track->samplespeed);
    printf("  Channels : %s\r\n", current_track->channels == 1 ? "Mono" : "Stereo");
    printf("  Header: %X\r\n", current_track->header);
    printf("  Start: %X\r\n", current_track->audio_start);
    printf("  Start: %X\r\n", current_track->audio_end);

    set_visualizer(temp_visualizer);

    // Pass it to the playback loop
    exitCode = jukebox();

    // play next song
    if (exitCode == 1){
        song_choice = (song_choice + 1) % track_count;
        printf("\r\n Next song!\r\n");
        dprint("Next song!");
    }
    // play previous song
    if (exitCode == 2){
        song_choice = (song_choice - 1 + track_count) % track_count;
        dprint("Prev Song!");
        printf("\r\nPrev Song!\r\n");
    }
    // play selected song in menu (visualizer 6)
    if (exitCode == 3){
        dprint("Playing picked Song!");
        printf("\r\nPlaying picked Song!\r\n");
    }

    return 0;
}

int browse_artists() {
    
}