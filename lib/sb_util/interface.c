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

uint16_t prev_choice = 0;
uint16_t song_choice = 0;

track_info_t track_window[10];
track_info_t *current_track = NULL;
track_info_t current_track_holder;

album_info_t album_window[10];
album_info_t *current_album = NULL;
album_info_t current_album_holder;

artist_info_t artist_window[10];
artist_info_t *current_artist = NULL;
artist_info_t current_artist_holder;

uint16_t song_choice;
uint16_t album_choice;
uint16_t artist_choice;
uint16_t menu_choice;

int temp_visualizer;

uint16_t browse_artists(int *exitCode) {
    current_artist = &current_artist_holder;
    if (!sb_get_artist_window(artist_choice, current_artist, artist_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    // read_lwbt();
    temp_visualizer = (visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (*exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        uint16_t selected = false;
        set_visualizer(4);
        prev_choice = artist_choice;
        while (selected == false) {
            switch (current_button_states) {
            case BTN_D:
                artist_choice = (artist_choice + 1) % artist_count;
                break;
            case BTN_U:
                artist_choice = (artist_choice - 1 + artist_count) % artist_count; //added roll-over
                break;
            case BTN_R:
                artist_choice = (artist_choice - 10 + artist_count) % artist_count;
                break;
            case BTN_L:
                artist_choice = (artist_choice + 10) % artist_count;
                break;
            case BTN_B:
                return 65535;
            case BTN_A:
                selected = 1;   
                printf("Poo cum fart shit pee\n");
            default:
                break;
            }
            sb_get_artist_window(artist_choice, current_artist, artist_window);
            if (prev_choice != artist_count){
                printf("\r\nArtist %d/%d: ", artist_choice+1, artist_count);
                prev_choice = artist_choice;
            }
            sleep_ms(100);
        }
    }

    // if (!sb_get_artist_window(artist_choice, current_artist, artist_window)) {
    //     printf("Error reading track metadata from cache table!\n");
    // }

    return current_artist->start_album;
}

int play_track(int *exitCode) {
    current_track = &current_track_holder;

    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
        printf("Error reading track metadata from cache table!\n");
    }

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
    *exitCode = jukebox(exitCode);

    // play next song
    if (*exitCode == 1){
        song_choice = (song_choice + 1) % track_count;
        printf("\r\n Next song!\r\n");
        dprint("Next song!");
    }
    // play previous song
    if (*exitCode == 2){
        song_choice = (song_choice - 1 + track_count) % track_count;
        dprint("Prev Song!");
        printf("\r\nPrev Song!\r\n");
    }
    // play selected song in menu (visualizer 6)
    if (*exitCode == 3){
        dprint("Playing picked Song!");
        printf("\r\nPlaying picked Song!\r\n");
    }

    return 0;
}

uint16_t browse_albums(int *exitCode) {
    current_track = &current_track_holder;
    current_album = &current_album_holder;
    if (!sb_get_album_window(album_choice, current_album, album_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    sb_get_track_window_fast(&tracks_cache_file, current_album->start_track, current_track, track_window);
    // read_lwbt();
    temp_visualizer = (visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (*exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        uint16_t selected = false;
        set_visualizer(5);
        prev_choice = album_choice;
        while (selected == false) {
            switch (current_button_states) {
            case BTN_D:
                album_choice = (album_choice + 1) % album_count;
                break;
            case BTN_U:
                album_choice = (album_choice - 1 + album_count) % album_count; //added roll-over
                break;
            case BTN_R:
                album_choice = (album_choice - 10 + album_count) % album_count;
                break;
            case BTN_L:
                album_choice = (album_choice + 10) % album_count;
                break;
            case BTN_B:
                return 65535;
            case BTN_A:
                selected = 1;   
                printf("Poo cum fart shit pee\n");
            default:
                break;
            }
            sb_get_album_window(album_choice, current_album, album_window);
            sb_get_track_window_fast(&tracks_cache_file, current_album->start_track, current_track, track_window);
            if (prev_choice != album_choice){
                printf("\r\nAlbum Fuck %d/%d: ", album_choice+1, album_count);
                prev_choice = album_choice;
            }
            sleep_ms(100);
        }
    }

    // if (!sb_get_album_window(album_choice, current_album, album_window)) {
    //     printf("Error reading track metadata from cache table!\n");
    // }

    return current_album->start_track;
}

uint16_t browse_tracks(int *exitCode) {
    current_track = &current_track_holder;
    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    // read_lwbt();
    temp_visualizer = (visualizer == 5 || visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (*exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        uint16_t selected = false;
        set_visualizer(6);
        printf("\r\nSong %d/%d: ", song_choice+1, track_count);
        prev_choice = song_choice;
        while (selected == false) {
            switch (current_button_states) {
            case BTN_D:
                song_choice = (song_choice + 1) % track_count;
                break;
            case BTN_U:
                song_choice = (song_choice - 1 + track_count) % track_count; //added roll-over
                break;
            case BTN_R:
                song_choice = (song_choice - 10 + track_count) % track_count;
                break;
            case BTN_L:
                song_choice = (song_choice + 10) % track_count;
                break;
            case BTN_B:
                return 65535;
            case BTN_A:
                selected = 1;   
                printf("Poo cum fart shit pee\n");
            default:
                break;
            }
            sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window);
            if (prev_choice != song_choice){
                printf("\r\nSong %d/%d: ", song_choice+1, track_count);
                prev_choice = song_choice;
            }
            
            sleep_ms(100);
        }
    }

    play_track(exitCode);
    return 0;
}