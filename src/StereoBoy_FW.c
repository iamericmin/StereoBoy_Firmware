#include "lib/sb_util/global_vars.h"

#include "lib/sb_util/sb_util.h"
#include "lib/buttons/buttons.h"
#include "lib/pot/pot.h"
#include "lib/radiomag/radiomag_util.h"

#include "pico/stdlib.h"
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

track_info_t tracks[MAX_TRACKS];
int song_choice = 0;
int count;
float x_brightness = 0.5;

int main()
{
    set_visualizer(7);
    // Lower RP2350 core voltage to 1V
    // P = V^2 * f, so 0.1V drop results in quadratic change
    // Before: 1.1 ^ 2 * 150 = 181.5
    // Now: 1.0 ^ 2 * 150 = 150
    vreg_set_voltage(VREG_VOLTAGE_1_00);

    stdio_init_all();

    // sleep_ms(3000);

    sb_hw_init(&player, &display);
    
    // Boot-up banner

    sleep_ms(250);

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

    dprint("Starting Track Scan");
    // pause_core1();
    sb_scan_tracks(tracks, MAX_TRACKS); //Implicitly sets count now
    // resume_core1();
    int exitCode = 0;
    int prev_choice = 0;
    bool selected = 0;
    bool inRadio = 0;
    // --- Print menu ---
    dprint("Debug print test %d", 1); //Trigger Core 2 Print
    printf("Debug print test %s\r\n", "2");
    
    song_choice = 0;
    
    while(1) {
        read_lwbt();
        //Return to main menu with list selection:
        if (exitCode == 0) {
            selected = false; 
            set_visualizer(6);
            bool confirmed = 0;
            clear_framebuffer();
            printf("\r\nSong %d/%d: ", song_choice+1, count);
            prev_choice = song_choice;
            while (selected == false) {
                uint8_t pressed = buttons_get_just_pressed();
                if (pressed > 0){
                    if (pressed & BTN_R)      choice = (choice + 1) % count;
                    if (pressed & BTN_L)      choice = (choice - 1 + count) % count; //added roll-over
                    if (pressed & BTN_U)      choice = (choice + 10) % count;
                    if (pressed & BTN_D)      choice = (choice - 10 + (count * 10)) % count; //added roll-over
                    if (pressed & BTN_A)      selected = true;   
                    if (pressed & BTN_SELECT) inRadio = radioLoop(&player);
                       
                }
                if ((prev_choice != choice)){
                    clear_framebuffer();
                    dprint("Song %d/%d: ", choice+1, count);
                    printf("\r\nSong %d/%d: ", choice+1, count);

                    dprint("%s", tracks[choice].title);
                    dprint("%s", tracks[choice].artist);
                    prev_choice = choice;
                uint8_t maped_btn = buttons_map_menu_navigation();
                uint8_t btn = get_button_repeat(maped_btn);
                    if (btn == 'd')      song_choice = (song_choice + 1) % count;
                    if (btn == 'u')      song_choice = (song_choice - 1 + count) % count; //added roll-over
                    if (btn == 'r')      song_choice = (song_choice + 10) % count;
                    if (btn == 'l')      song_choice = (song_choice - 10 + count) % count;
                    if (btn == 'p')      selected = true;
                    if (btn == 'm')      song_choice = (rand() % count);   
                if (prev_choice != song_choice){
                    printf("\r\nSong %d/%d: ", song_choice+1, count);
                    prev_choice = song_choice;
                }
                if (inRadio){
                    clear_framebuffer();
                    dprint("Song %d/%d: ", choice+1, count);
                    printf("\r\nSong %d/%d: ", choice+1, count);

                    dprint("%s", tracks[choice].title);
                    dprint("%s", tracks[choice].artist);
                    inRadio = 0;
                }
                
                sleep_ms(10);
            }
        }
        track_info_t *track = &tracks[song_choice];

        printf("\r\n\rNOW PLAYING:\r\n");
        printf("  Title : %s\r\n", track->title);
        printf("  Artist: %s\r\n", track->artist);
        printf("  Album : %s\r\n", track->album);
        printf("  Bitrate : %d Kbps\r\n", track->bitrate);
        printf("  Sample rate : %d Hz\r\n", track->samplespeed);
        printf("  Channels : %s\r\n", track->channels == 1 ? "Mono" : "Stereo");
        printf("  Header: %X\r\n", track->header);
        printf("  Start: %X\r\n", track->audio_start);
        printf("  Start: %X\r\n", track->audio_end);

        set_visualizer(1);
        exitCode = jukebox(&player, track, &display);

        if (exitCode == 1){
            if (song_choice + 1 > count)
                song_choice = 1;
            else
                song_choice += 1;
            printf("\r\n Next song!\r\n");
            dprint("Next song!");
        }
        if (exitCode == 2){
            if (song_choice - 1 < 1)
                song_choice = count;
            else
                song_choice -= 1;
            dprint("Prev Song!");
            printf("\r\nPrev Song!\r\n");
        }
    }
}
