#include "lib/sb_util/global_vars.h"

#include "lib/sb_util/sb_util.h"
#include "lib/buttons/buttons.h"
#include "lib/pot/pot.h"

#include "pico/stdlib.h"
#include <stdint.h>
#include "stdio.h"
#include "ff.h"
#include "hardware/vreg.h"
#include "lib/sb_util/interface.h"

// SPI1 configuration for codec & sd card
#define PIN_SCK  30
#define PIN_MOSI 28
#define PIN_MISO 31
#define PIN_CS   32

// Codec control signals
#define PIN_DCS  33
#define PIN_DREQ 29
#define PIN_RST  27

// I2C0 for DAC
#define PIN_I2C0_SCL 21
#define PIN_I2C0_SDA 20

vs1053_t player = {
    .spi = spi1,
    .cs = PIN_CS,
    .dcs = PIN_DCS,
    .dreq = PIN_DREQ,
    .rst = PIN_RST
};

st7789_t display = {
    .spi      = spi0,
    .gpio_din = 35,
    .gpio_clk = 34,
    .gpio_cs  = 37,
    .gpio_dc  = 39,
    .gpio_rst = 4,
    .gpio_bl  = 5,
};

#define LCD_WIDTH  240
#define LCD_HEIGHT 240

folder_info_t folders[MAX_FOLDERS];

track_info_t track_window[10];
track_info_t *current_track = NULL;
track_info_t current_track_holder;

album_info_t album_window[10];
album_info_t *current_album = NULL;
album_info_t current_album_holder;

artist_info_t artist_window[10];
artist_info_t *current_artist = NULL;
artist_info_t current_artist_holder;

// file that contains all tracks' metadata
// VERY IMPORTANT
FIL tracks_cache_file;

char folder_names[20][64];
int folder_file_counts[20];

uint16_t song_choice = 0;
uint16_t album_choice = 0;
uint16_t artist_choice = 0;
uint16_t prev_choice = 0;
uint16_t menu_choice = 0;
uint8_t selected = 0;

int temp_visualizer = 6;
int exitCode = 0;

uint16_t browse_artists() {
    current_artist = &current_artist_holder;
    if (!sb_get_artist_window(artist_choice, current_artist, artist_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    // read_lwbt();
    temp_visualizer = (visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        selected = false;
        set_visualizer(4);
        prev_choice = artist_choice;
        while (selected == false) {
            uint8_t pressed = buttons_get_just_pressed();
            if (pressed > 0){
                if (pressed & BTN_D)      artist_choice = (artist_choice + 1) % artist_count;
                if (pressed & BTN_U)      artist_choice = (artist_choice - 1 + artist_count) % artist_count; //added roll-over
                    // shift songs down by one and insert new song on top
                if (pressed & BTN_R)      artist_choice = (artist_choice + 10) % artist_count;
                if (pressed & BTN_L)      artist_choice = (artist_choice - 10 + artist_count) % artist_count;
                if (pressed & BTN_B) {
                    return 100;
                }
                if (pressed & BTN_A) {
                    selected = true;   
                    printf("Poo cum fart shit pee\n");
                }       
                sb_get_artist_window(artist_choice, current_artist, artist_window);
            }
            if (prev_choice != artist_choice){
                printf("\r\nArtist %d/%d: ", artist_choice+1, artist_count);
                prev_choice = artist_choice;
            }
            
            sleep_ms(10);
        }
    }

    if (!sb_get_artist_window(artist_choice, current_artist, artist_window)) {
        printf("Error reading track metadata from cache table!\n");
    }

    return current_artist->start_album;
}

uint16_t browse_albums() {
    current_album = &current_album_holder;
    if (!sb_get_album_window(album_choice, current_album, album_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    sb_get_track_window_fast(&tracks_cache_file, current_album->start_track, current_track, track_window);
    // read_lwbt();
    temp_visualizer = (visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        selected = false;
        set_visualizer(5);
        prev_choice = album_choice;
        while (selected == false) {
            uint8_t pressed = buttons_get_just_pressed();
            if (pressed > 0){
                if (pressed & BTN_D)      album_choice = (album_choice + 1) % album_count;
                if (pressed & BTN_U)      album_choice = (album_choice - 1 + album_count) % album_count; //added roll-over
                    // shift songs down by one and insert new song on top
                if (pressed & BTN_R)      album_choice = (album_choice + 10) % album_count;
                if (pressed & BTN_L)      album_choice = (album_choice - 10 + album_count) % album_count;
                if (pressed & BTN_B) {
                    return 100;
                }
                if (pressed & BTN_A) {
                    selected = true;   
                    printf("Poo cum fart shit pee\n");
                }       
                sb_get_album_window(album_choice, current_album, album_window);
                sb_get_track_window_fast(&tracks_cache_file, current_album->start_track, current_track, track_window);
            }
            if (prev_choice != album_choice){
                printf("\r\nAlbum Fuck %d/%d: ", album_choice+1, album_count);
                prev_choice = album_choice;
            }
            
            sleep_ms(10);
        }
    }

    if (!sb_get_album_window(album_choice, current_album, album_window)) {
        printf("Error reading track metadata from cache table!\n");
    }

    return current_album->start_track;
}

int browse_tracks() {
    current_track = &current_track_holder;
    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    // read_lwbt();
    temp_visualizer = (visualizer == 5 || visualizer == 7) ? 1 : visualizer;
    //Return to main menu with list selection:
    if (exitCode == 0) {
        // pca9685_all_off(&vu_meter);
        selected = false;
        set_visualizer(6);
        printf("\r\nSong %d/%d: ", song_choice+1, track_count);
        prev_choice = song_choice;
        while (selected == false) {
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
                    selected = true;   
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
    exitCode = jukebox(&player, current_track, &display);

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

int main() {
    // set_visualizer(6);
    // Lower RP2350 core voltage to 1V
    // P = V^2 * f, so 0.1V drop results in quadratic change
    // Before: 1.1 ^ 2 * 150 = 181.5
    // Now: 1.0 ^ 2 * 150 = 150
    vreg_set_voltage(VREG_VOLTAGE_1_00);

    stdio_init_all();

    sb_hw_init(&player, &display);
    
    // Boot-up banner

    // printf("\033c"); // clear screen

    printf(R"(
   _____ __                       ____             
  / ___// /____  ________  ____  / __ )____  __  __ 
  \__ \/ __/ _ \/ ___/ _ \/ __ \/ __  / __ \/ / / /
 ___/ / /_/  __/ /  /  __/ /_/ / /_/ / /_/ / /_/ / 
/____/\__/\___/_/   \___/\____/_____/\____/\__, /  
   MODULAR SUPER HI-FI STEREO SYSTEM      /____/
   ENGINEERING PROTOTYPE UNIT 001)");
    printf("\r\n\r\n");

    // sleep_ms(750); // pause for dramatic effect

    // printf("Starting Folder Scan...\n");

    // uint8_t total_folders = sb_scan_folders(folders, 20);
    // printf("--- Found %d Folders ---\n", total_folders);
    // for (int i = 0; i < total_folders; i++) {
    //     printf("[%02d] %-16s (%d songs)\n", 
    //             i, 
    //             folders[i].foldername, 
    //             folders[i].num_tracks);
    // }

    dprint("Starting Track Scan");

    // Load your main relational pointers into RAM first
    sb_load_library();
    if (sb_load_tracks_cache(&tracks_cache_file) != FR_OK) {
        while(1) {
            printf("What the fuck just happened\n");
            sleep_ms(1000);
        }
    }

    printf("%d Artists\n", artist_count);
    printf("%d Albums\n", album_count);
    printf("%d Tracks\n", track_count);

    menu_choice = 1;
    selected = 100;

    while (1) {
        menu_choice = 1;
        selected = 100;
        
        set_visualizer(7);
        while(selected == 100) {
            uint8_t pressed = buttons_get_just_pressed();
            if (pressed > 0){
                if (pressed & BTN_D)      menu_choice = (menu_choice + 1);
                if (pressed & BTN_U)      menu_choice = (menu_choice - 1);
                if (pressed & BTN_A){
                    selected = 1;   
                    printf("Poo cum fart shit pee\n");
                }
                if (menu_choice < 1) {
                    menu_choice = 1;
                } else if (menu_choice > 7) {
                    menu_choice = 7;
                }
            }
            sleep_ms(50);
        }

        switch (menu_choice) {
            // Artists
            case 1:
                selected = 0;
                album_choice = browse_artists();
                selected = 0;
                song_choice = browse_albums();
                break;
            // Albums
            case 2:
                selected = 0;
                song_choice = browse_albums();
                break;
            // Tracks
            case 3:
                break;
            // Last Played
            case 4:
                break;
            // Shuffle All
            case 5:
                break;
            
            default:
                break;
        }
        selected = 0;
        int result = 0;
        while (result != 100) {
            result = browse_tracks();
        }
    }
}
