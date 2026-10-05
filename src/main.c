#include "lib/sb_util/global_vars.h"

#include "lib/sb_util/sb_util.h"
#include "lib/buttons/buttons.h"
#include "lib/pot/pot.h"

#include "pico/stdlib.h"
#include <stdint.h>
#include "stdio.h"
#include "ff.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "lib/sb_util/interface.h"
#include "lib/fram/fram.h"

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

uint16_t song_choice = 0;
uint16_t album_choice = 0;
uint16_t artist_choice = 0;

uint16_t last_played_track_idx = 0;
uint16_t last_played_album_idx = 0;
uint16_t last_played_artist_idx = 0;

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

folder_info_t folders[MAX_FOLDERS];

// file that contains all tracks' metadata
// VERY IMPORTANT
FIL tracks_cache_file;

char folder_names[20][64];
int folder_file_counts[20];

int exitCode = 0;

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

    printf("\033c"); // clear screen

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

    // parse .sbc cache files and load library metadata
    sb_load_library();
    if (sb_load_tracks_cache(&tracks_cache_file) != FR_OK) {
        while(1) {
            printf("What the fuck just happened\n");
            sleep_ms(1000);
        }
    }

    last_played_track_holder = current_track_holder; 
    last_played_track = &last_played_track_holder;

    // uint32_t last_played_pos = 0;
    // read last played song and fetch its metadata
    int status = fram_read(i2c0, 0x0000, (uint8_t*)&last_played_track_idx, sizeof(last_played_track_idx));
    if (status > 0 && last_played_track_idx < track_count) {
        // song_choice = last_played_track_idx;
        printf("[F-RAM] Loaded last track: %d\n", song_choice);
    } else {
        printf("[F-RAM Warning] Invalid read (%d) or bus failure. Defaulting to track 0.\n", last_played_track_idx);
        song_choice = 0; // Fallback to track 0 safely
    }

    if (!sb_get_track_window_fast(&tracks_cache_file, last_played_track_idx, last_played_track, track_window)) {
        printf("Error reading track metadata from cache table!\n");
    }

    printf("%d Artists\n", artist_count);
    printf("%d Albums\n", album_count);
    printf("%d Tracks\n", track_count);

    menu_choice = 1;
    uint16_t selected = 65535;

    while (1) {
        exitCode = 0;
        menu_choice = 1;
        selected = 65535;
        
        set_visualizer(7);
        while(selected == 65535) {
            // TODO: add left and right for fast scrolling
            switch (buttons_get_action()) {
                case BTN_D:
                    menu_choice = (menu_choice + 1);
                    break;
                case BTN_U:
                    menu_choice = (menu_choice - 1);
                    break;
                case BTN_A:
                    selected = 1;   
                    printf("Main: Poo cum fart shit pee\n");
                    break;
                default:
                    break;
            }
            if (menu_choice < 1) {
                menu_choice = 1;
            } else if (menu_choice > 7) {
                menu_choice = 7;
            }
            sleep_ms(50);
        }

        switch (menu_choice) {
            // Artists -> Albums -> Tracks
            case 1: {
                selected = 0;
                uint16_t start_album = browse_artists(&exitCode);
                if (start_album == 65535) {
                    break;
                }
                album_choice = start_album;

                selected = 0;
                uint16_t start_track = browse_albums(&exitCode);
                if (start_track == 65535) {
                    break;
                }
                song_choice = start_track;

                int result = 0;
                while (result != 65535) {
                    result = browse_tracks(&exitCode);
                }
                break;
            }

            // Albums -> Tracks
            case 2: {
                selected = 0;
                uint16_t start_track = browse_albums(&exitCode);
                if (start_track == 65535) {
                    break;
                }
                song_choice = start_track;

                int result = 0;
                while (result != 65535) {
                    result = browse_tracks(&exitCode);
                }
                break;
            }

            // Tracks (Direct Browse)
            case 3: {
                int result = 0;
                while (result != 65535) {
                    result = browse_tracks(&exitCode);
                }
                break;
            }
            // Last Played
            case 4: {
                song_choice = last_played_track_idx;
                album_choice = last_played_album_idx;
                artist_choice = last_played_artist_idx;
                exitCode = -1; // if exitCode != 0, browse_tracks skips the selection menu and goes straight to jukebox
                int result = 0;
                while (result != 65535) {
                    result = browse_tracks(&exitCode);
                }
                break;
            }
            // Shuffle All
            case 5:
                break;
            
            default:
                break;
        }
    }
}
