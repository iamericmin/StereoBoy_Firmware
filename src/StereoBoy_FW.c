#include "lib/sb_util/global_vars.h"

#include "lib/sb_util/sb_util.h"
#include "lib/buttons/buttons.h"
#include "lib/pot/pot.h"

#include "pico/stdlib.h"
#include "stdio.h"
#include "ff.h"
#include "hardware/vreg.h"

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

struct st7789_t display = {
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

track_info_t *current_track = NULL;
track_info_t current_track_holder;

char folder_names[20][64];
int folder_file_counts[20];

int song_choice = 0;

int temp_visualizer = 1;

int main() {
    set_visualizer(1);
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

    dprint("Starting Folder Scan");

    uint8_t total_folders = sb_scan_folders(folders, 20);
    printf("--- Found %d Folders ---\n", total_folders);
    for (int i = 0; i < total_folders; i++) {
        printf("[%02d] %-16s (%d songs)\n", 
                i, 
                folders[i].foldername, 
                folders[i].num_tracks);
    }

    dprint("Starting Track Scan");

    // Load your main relational pointers into RAM first
    sb_load_library();

    printf("%d Artists\n", artist_count);
    printf("%d Albums\n", album_count);
    printf("%d Tracks\n", track_count);
    
    // FIL db_fil;
    // UINT br;
    // int index = 0;

    // printf("\n--- VERIFYING HOST-SIDE CACHE GENERATION ---\n");
    // printf("\nPRESS ANY KEY TO CONTINUE...\n");
    // while (buttons_get_just_pressed() <= 0);
    // printf("\nLISTING ALL ALBUMS\n\n");
    // // Open the updated .tracks file created by your Python script
    // if (f_open(&db_fil, "0:/.albums", FA_READ) == FR_OK)
    // {
        
    //     // Single 384-byte container allocated locally on the CPU stack frame
    //     album_info_t t; 

    //     // Read sequentially until we reach the End-of-File boundary
    //     while (f_read(&db_fil, &t, sizeof(album_info_t), &br) == FR_OK && br == sizeof(album_info_t))
    //     {
    //         printf("[%03d] %s - %d songs. Starting track index: %d\n", index, t.album_name, t.num_tracks, t.start_track);
    //         index++;
    //     }
    //     f_close(&db_fil);
    //     printf("--- End of List (%d tracks validated clean) ---\n\n", index);
    // } 
    // else 
    // {
    //     printf("[Fatal Error] Could not open 0:/.albums for validation readout.\n");
    // }
    // printf("\nPRESS ANY KEY TO CONTINUE...\n");
    // while (buttons_get_just_pressed() <= 0);
    // printf("\nLISTING ALL ARTISTS\n\n");
    // index = 0;
    // // Open the updated .tracks file created by your Python script
    // if (f_open(&db_fil, "0:/.artists", FA_READ) == FR_OK)
    // {
        
    //     // Single 384-byte container allocated locally on the CPU stack frame
    //     artist_info_t t; 

    //     // Read sequentially until we reach the End-of-File boundary
    //     while (f_read(&db_fil, &t, sizeof(artist_info_t), &br) == FR_OK && br == sizeof(artist_info_t))
    //     {
    //         printf("[%03d] %s - %d songs. Starting album index: %d\n", index, t.artist_name, t.num_albums, t.start_album);
    //         index++;
    //     }
    //     f_close(&db_fil);
    //     printf("--- End of List (%d tracks validated clean) ---\n\n", index);
    // } 
    // else 
    // {
    //     printf("[Fatal Error] Could not open 0:/.artists for validation readout.\n");
    // }
    // printf("\nPRESS ANY KEY TO CONTINUE...\n");
    // while (buttons_get_just_pressed() <= 0);
    // printf("\nLISTING ALL TRACKS\n");
    // index = 0;
    // // Open the updated .tracks file created by your Python script
    // if (f_open(&db_fil, "0:/.tracks", FA_READ) == FR_OK)
    // {
        
    //     // Single 384-byte container allocated locally on the CPU stack frame
    //     track_info_t t; 

    //     // Read sequentially until we reach the End-of-File boundary
    //     while (f_read(&db_fil, &t, sizeof(track_info_t), &br) == FR_OK && br == sizeof(track_info_t))
    //     {
    //         printf("[%03d] %s by %s\n", index, t.title, t.artist);
    //         printf("  Album : %s\r\n", t.album);
    //         printf("  Filename : %s\r\n", t.filename);
    //         printf("  ============================================\r\n");
    //         index++;
    //     }
    //     f_close(&db_fil);
    //     printf("--- End of List (%d tracks validated clean) ---\n\n", index);
    // } 
    // else 
    // {
    //     printf("[Fatal Error] Could not open 0:/.tracks for validation readout.\n");
    // }

    int exitCode = 0;
    int prev_choice = 0;
    bool selected = 0;
    
    song_choice = 0;
    current_track = &current_track_holder;
    if (!sb_get_track_by_index(song_choice, current_track, track_window)) {
        printf("Error reading track metadata from cache table!\n");
    }
    
    while(1) {
        // read_lwbt();
        temp_visualizer = (visualizer == 7) ? 1 : visualizer;
        //Return to main menu with list selection:
        if (exitCode == 0) {
            // pca9685_all_off(&vu_meter);
            selected = false;
            set_visualizer(1);
            printf("\r\nSong %d/%d: ", song_choice+1, count);
            prev_choice = song_choice;
            while (selected == false) {
                uint8_t pressed = buttons_get_just_pressed();
                if (pressed > 0){
                    if (pressed & BTN_D)      song_choice = (song_choice + 1) % count;
                    if (pressed & BTN_U)      song_choice = (song_choice - 1 + count) % count; //added roll-over
                        // shift songs down by one and insert new song on top
                    if (pressed & BTN_R)      song_choice = (song_choice + 10) % count;
                    if (pressed & BTN_L)      song_choice = (song_choice - 10 + count) % count;
                    if (pressed & BTN_A){
                        selected = true;   
                        printf("Poo cum fart shit pee\n");
                    }       
                }
                if (prev_choice != song_choice){
                    printf("\r\nSong %d/%d: ", song_choice+1, count);
                    prev_choice = song_choice;
                }
                
                sleep_ms(10);
            }
        }

        // track_info_t track_buff;

        // Fetch strings directly from the SD card into our globally visible runtime tracking struct
        // if (!sb_get_track_by_index(song_choice, &current_track)) {
        //     printf("Error reading track metadata from cache table!\n");
        //     continue; 
        // }

        // Parse metadata directly into the shared global structure
        // get_mp3_metadata(current_track.filename, &current_track); 

        set_visualizer(temp_visualizer);
        
        // Pass it to the playback loop
        exitCode = jukebox(&player, song_choice, &display);

        // play next song
        if (exitCode == 1){
            song_choice = (song_choice + 1) % count;
            printf("\r\n Next song!\r\n");
            dprint("Next song!");
        }
        // play previous song
        if (exitCode == 2){
            song_choice = (song_choice - 1 + count) % count;
            dprint("Prev Song!");
            printf("\r\nPrev Song!\r\n");
        }
        // play selected song in menu (visualizer 6)
        if (exitCode == 3){
            dprint("Playing picked Song!");
            printf("\r\nPlaying picked Song!\r\n");
        }
    }
}
