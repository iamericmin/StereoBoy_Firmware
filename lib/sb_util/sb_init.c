#include "global_vars.h"

#include "lib/sb_util/sb_util.h"
#include "hardware/pwm.h"
#include "hardware/xip_cache.h"

static FATFS fs;

static int dma_chan = -1;
static dma_channel_config dcc;

LUT_entry_t *artCache_LUT = NULL;
uint32_t lut_entry_count = 0;

// Define the variables cleanly (no extern here!)
artist_info_t *global_artists = NULL;
album_info_t  *global_albums  = NULL;

uint16_t artist_count = 0;
uint16_t album_count  = 0;
uint16_t track_count  = 0;

void set_backlight_brightness(uint gpio, uint16_t brightness_percent) {
    // Ensure percent is clamped between 0 and 100
    if (brightness_percent > 100) brightness_percent = 100;

    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint chan = pwm_gpio_to_channel(gpio);
    
    // Initial setup (only needs to be done once, but safe to repeat)
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    
    // Using a wrap of 255 for 8-bit-like resolution, 
    // or 10000 for finer control. Let's use 1000.
    uint16_t wrap = 1000;
    pwm_set_wrap(slice_num, wrap);
    
    // Calculate level based on percentage
    uint16_t level = (brightness_percent * wrap) / 100;
    
    pwm_set_chan_level(slice_num, chan, level);
    pwm_set_enabled(slice_num, true);
}

void st7789_init(const struct st7789_t* config, uint16_t width, uint16_t height)
{
    memcpy(&st7789_cfg, config, sizeof(st7789_cfg));
    st7789_width = width;
    st7789_height = height;

    spi_init(st7789_cfg.spi, 150 * 1000 * 1000);
    if (st7789_cfg.gpio_cs > -1) {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    } else {
        spi_set_format(st7789_cfg.spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    }

    gpio_set_function(st7789_cfg.gpio_din, GPIO_FUNC_SPI);
    gpio_set_function(st7789_cfg.gpio_clk, GPIO_FUNC_SPI);

    if (st7789_cfg.gpio_cs > -1) {
        gpio_init(st7789_cfg.gpio_cs);
    }
    gpio_init(st7789_cfg.gpio_dc);
    gpio_init(st7789_cfg.gpio_rst);
    gpio_init(st7789_cfg.gpio_bl);

    if (st7789_cfg.gpio_cs > -1) {
        gpio_set_dir(st7789_cfg.gpio_cs, GPIO_OUT);
    }
    gpio_set_dir(st7789_cfg.gpio_dc, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_rst, GPIO_OUT);
    gpio_set_dir(st7789_cfg.gpio_bl, GPIO_OUT);

    if (st7789_cfg.gpio_cs > -1) {
        gpio_put(st7789_cfg.gpio_cs, 1);
    }
    gpio_put(st7789_cfg.gpio_dc, 1);
    gpio_put(st7789_cfg.gpio_rst, 1);
    // sleep_ms(10);
    
    // SWRESET (01h): Software Reset
    st7789_cmd(0x01, NULL, 0);
    // sleep_ms(15);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(0x11, NULL, 0);
    // sleep_ms(10);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 16bit/pixel
    st7789_cmd(0x3a, (uint8_t[]){ 0x55 }, 1);
    // sleep_ms(10);

    // MADCTL (36h): Memory Data Access Control
    // - Page Address Order            = Top to Bottom
    // - Column Address Order          = Left to Right
    // - Page/Column Order             = Normal Mode
    // - Line Address Order            = LCD Refresh Top to Bottom
    // - RGB/BGR Order                 = RGB
    // - Display Data Latch Data Order = LCD Refresh Left to Right
    st7789_cmd(0x36, (uint8_t[]){ 0x00 }, 1);
   
    st7789_caset(0, width);
    st7789_raset(0, height);

    // INVON (21h): Display Inversion On
    st7789_cmd(0x21, NULL, 0);
    // sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(0x13, NULL, 0);
    // sleep_ms(10);

    // DISPON (29h): Display On
    st7789_cmd(0x29, NULL, 0);
    // sleep_ms(10);

    set_backlight_brightness(st7789_cfg.gpio_bl, 50);
}

void sb_display_init(st7789_t *display)
{
    st7789_init(display, SCREEN_WIDTH, SCREEN_HEIGHT);
    printf("Display initialized!\r\n");

    // Setup DMA for super-fast draw routines
    dma_chan = dma_claim_unused_channel(true);
    dcc = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dcc, DMA_SIZE_16);
    channel_config_set_dreq(&dcc, spi_get_dreq(display->spi, true));
    // 1. Fill the entire buffer with zeros (Black) instantly
    // Each pixel is 2 bytes, so total size is 240 * 240 * 2
    memset(frame_buffer, 0, sizeof(frame_buffer));

    // 2. Set the display window to the full screen
    st7789_set_cursor(0, 0);
    st7789_ramwr();

    // 3. Ensure SPI is in 16-bit mode for the DMA transfer
    spi_set_format(display->spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    // 4. Use DMA to push the black buffer to the display
    // This returns almost immediately while the hardware does the work
    dma_channel_configure(
        dma_chan,
        &dcc,
        &spi_get_hw(display->spi)->dr, // Destination: SPI TX register
        frame_buffer,                  // Source: Our cleared RAM buffer
        240 * 240,                     // Count: Total number of 16-bit pixels
        true                           // Start now!
    );
    // sleep_ms(500);
}

// scans for folders in root dir
// also parses their name and # of items as metadata
int sb_scan_folders(folder_info_t *folders, int max_folders) {
    DIR dir;
    FILINFO fno;
    int folder_count = 0;

    if (f_opendir(&dir, "0:/") != FR_OK) {
        return 0; 
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0) {
        // Filter for directories, skipping hidden/system ones
        if ((fno.fattrib & AM_DIR) && (fno.fname[0] != '.')) {
            if (folder_count < max_folders) {
                // Populate Name
                strncpy(folders[folder_count].foldername, fno.fname, 63);
                folders[folder_count].foldername[63] = '\0';
                folders[folder_count].num_tracks = 0;

                // Count audio files inside
                DIR sub_dir;
                FILINFO sub_fno;
                char path[128];
                snprintf(path, sizeof(path), "0:/%s", fno.fname);

                if (f_opendir(&sub_dir, path) == FR_OK) {
                    while (f_readdir(&sub_dir, &sub_fno) == FR_OK && sub_fno.fname[0] != 0) {
                        if (!(sub_fno.fattrib & AM_DIR)) {
                            char *ext = strrchr(sub_fno.fname, '.');
                            if (ext && (!strcasecmp(ext, ".mp3") || !strcasecmp(ext, ".flac") || !strcasecmp(ext, ".wav"))) {
                                folders[folder_count].num_tracks++;
                            }
                        }
                    }
                    f_closedir(&sub_dir);
                }
                folder_count++;
            }
        }
    }
    f_closedir(&dir);

    // --- Internal Sort ---
    if (folder_count > 1) {
        qsort(folders, folder_count, sizeof(folder_info_t), compare_folders);
    }

    return folder_count;
}

// This function scans the current directory for all MP3 files
// and quickly generates an array of all their filenames
// Made this to make initial directory parsing faster
int sb_get_raw_tracks(char raw_tracks[][256], int max_tracks) {
    DIR dir;
    FILINFO fno;
    int count = 0;

    if (f_opendir(&dir, "0:/") != FR_OK) {
        return 0; 
    }

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != 0)
    {
        // Skip directories
        if (fno.fattrib & AM_DIR)
            continue;

        char *ext = strrchr(fno.fname, '.');
        if (ext && !strcasecmp(ext, ".mp3") && count < max_tracks)
        {
            // Copy filename into the current slot, then increment
            strncpy(raw_tracks[count], fno.fname, 255);
            raw_tracks[count][255] = '\0'; // Safety null-terminator
            count++;
        }
    }

    f_closedir(&dir);

    if (count == 0)
    {
        printf("No MP3 files found.\r\n");
        return 0;
    }

    qsort(raw_tracks, count, 256, compare_filenames_raw);
    return count;
}

/**
 * @brief Seeks the cache file on disk and extracts a specific track profile.
 * Directly populates the full technical struct with zero processing overhead.
 * @param idx The cache index to fetch the song from
 * @param out_track Pointer to the track container used by your jukebox.
 * @param track_window Pointer to an array of 11 track structures forming a UI slider layout window.
 * @return int 1 on success, 0 on disk read failure.
 */
int sb_get_track_by_index(uint16_t idx, track_info_t *out_track, track_info_t *track_window) {
    printf("Track fetch process started!\n");
    uint64_t initial_timestamp = get_absolute_time();
    FIL db_fil;
    UINT br; 

    // Open the flat index cache database file
    if (f_open(&db_fil, "0:/.tracks.sbc", FA_READ) != FR_OK) {
        printf("[Error] Failed to open .tracks.sbc cache file for lookup.\n");
        return 0;
    }
    printf("+ %d us! (Opened .tracks.sbc cache file)\n", (int)absolute_time_diff_us(initial_timestamp, get_absolute_time()));
    initial_timestamp = get_absolute_time();

    // 1. Instantly pull down the full selected song metadata block in one single disk operation
    uint32_t byte_offset = (uint32_t)idx * sizeof(track_info_t);
    f_lseek(&db_fil, byte_offset);

    if (f_read(&db_fil, out_track, sizeof(track_info_t), &br) != FR_OK || br != sizeof(track_info_t)) {
        f_close(&db_fil);
        return 0; // Failed to read target track row
    }
    printf("+ %d us! (Fetched whole track profile completely from disk cache)\n", (int)absolute_time_diff_us(initial_timestamp, get_absolute_time()));
    initial_timestamp = get_absolute_time();

    // 2. Populate the sliding track window (11 tracks total: 5 below, current, 5 above)
    uint32_t total_tracks = f_size(&db_fil) / sizeof(track_info_t);
    int32_t start_idx = (int32_t)idx - 5;
    
    for (int i = 0; i < 11; i++) {
        int32_t current_window_idx = start_idx + i;
        
        if (current_window_idx >= 0 && current_window_idx < (int32_t)total_tracks) {
            uint32_t window_offset = (uint32_t)current_window_idx * sizeof(track_info_t);
            f_lseek(&db_fil, window_offset);
            
            if (f_read(&db_fil, &track_window[i], sizeof(track_info_t), &br) != FR_OK || br != sizeof(track_info_t)) {
                memset(&track_window[i], 0, sizeof(track_info_t));
            }
        } else {
            // Out of bounds constraints (e.g. padding entries for list margins)
            memset(&track_window[i], 0, sizeof(track_info_t));
        }
    }

    f_close(&db_fil);
    printf("+ %d us! (Populated scrolling tracks window array)\n", (int)absolute_time_diff_us(initial_timestamp, get_absolute_time()));
    return 1;
}

// sb_update_track_window() {

// }

/**
 * @brief Zero-overhead binary loader for pre-compiled DAP index files.
 * @return int 1 on success, 0 on failure.
 */
int sb_load_library(void) {
    FILINFO fno;
    FIL fil;
    UINT br;

    // Safety clean reset
    if (global_artists != NULL) { free(global_artists); global_artists = NULL; }
    if (global_albums != NULL)  { free(global_albums);  global_albums = NULL;  }
    artist_count = 0;
    album_count = 0;

    printf("Loading music indexes into RAM...\r\n");

    // --- LOAD ARTISTS ---
    if (f_stat("0:/.artists.sbc", &fno) != FR_OK) {
        printf("[Error] 0:/.artists.sbc file missing.\r\n");
        return 0;
    }
    artist_count = fno.fsize / sizeof(artist_info_t);
    global_artists = (artist_info_t *)malloc(fno.fsize);
    if (global_artists == NULL) return 0;

    if (f_open(&fil, "0:/.artists.sbc", FA_READ) == FR_OK) {
        FRESULT res = f_read(&fil, global_artists, fno.fsize, &br);
        f_close(&fil);
        if (res != FR_OK || br != fno.fsize) {
            free(global_artists); global_artists = NULL;
            return 0;
        }
    }

    // --- LOAD ALBUMS ---
    if (f_stat("0:/.albums.sbc", &fno) != FR_OK) {
        printf("[Error] 0:/.albums.sbc file missing.\r\n");
        free(global_artists); global_artists = NULL;
        return 0;
    }
    album_count = fno.fsize / sizeof(album_info_t);
    global_albums = (album_info_t *)malloc(fno.fsize);
    if (global_albums == NULL) {
        free(global_albums); global_albums = NULL;
        return 0;
    }

    if (f_open(&fil, "0:/.albums.sbc", FA_READ) == FR_OK) {
        FRESULT res = f_read(&fil, global_albums, fno.fsize, &br);
        f_close(&fil);
        if (res != FR_OK || br != fno.fsize) {
            free(global_albums);  global_albums = NULL;
            return 0;
        }
    }

    // --- LOAD TRACKS ---
    if (f_stat("0:/.tracks.sbc", &fno) != FR_OK) {
        printf("[Error] 0:/.tracks.sbc file missing.\r\n");
        return 0;
    }
    track_count = fno.fsize / sizeof(track_info_t);

    printf("[Success] Loaded %d Artists, %d Albums, and %d tracks into RAM.\r\n", 
           artist_count, album_count, track_count);
    return 1;
}

void sb_hw_init(vs1053_t *player, st7789_t *display)
{
    mutex_init(&text_buff_mtx);
    sem_init(&text_sem, 0, 255);

    // sleep_ms(1000);

    // 1. Initialize the SDIO driver hardware parameters (PIO/DMA)
    bool sd_success = false;
    for (int i = 0; i < 50; i++) {
        if (sd_init_driver()) {
            sd_success = true;
            printf("SDIO card driver initialized on attempt %d!\r\n", i + 1);
            break; 
        }
        sleep_ms(100); // Give the card a moment before retrying
    }

    if (!sd_success) {
        printf("SDIO hardware init failed\r\n");
    }

    // 2. Mount via FatFS using the explicit volume ID "0:" defined in hw_config.c
    FRESULT fr;
    for (int retry = 0; retry < 100; retry++) {
        fr = f_mount(&fs, "0:", 1); // Forced immediate mount option
        if (fr == FR_OK) {
            printf("SDIO Card successfully mounted on try %d!\n", retry);
            break;
        } else {
            printf("Mount failed on try %d. Retrying...\n", retry);
            sleep_ms(50);
        }
    }
    if (fr != FR_OK) {
        while (1) {
            printf("SDIO Mount permanently failed: (%d)\n", fr);
            sleep_ms(1000);
        }
    }
    
    // set I2C0 for DAC at 400KHz
    gpio_set_function(PIN_I2C0_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C0_SDA, GPIO_FUNC_I2C);
    i2c_init(i2c0, 400 * 1000);
    printf("SPI0 and I2C0 initialized.\r\n");

    // set I2C1 for PCA9685 at 400KHz
    gpio_set_function(PIN_I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C1_SCL, GPIO_FUNC_I2C);
    i2c_init(i2c1, 400 * 1000);
    printf("I2C1 initialized.\r\n");

    // LED driver init
    if (pca9685_init(&vu_meter, i2c1, 0x40))
    {
        printf("PCA9685 LED Driver initialized!\r\n");
        pca9685_sleep(&vu_meter);
    }
    else
    {
        printf("WARNING: PCA9685 Init Failed!\r\n");
    }

    adc_init();        
    adc_gpio_init(46); // Left
    adc_gpio_init(45); // Right

    printf("Oscope ADC initialized!\r\n");

    sb_display_init(display);
    printf("test point 1");

    // initialize DAC
    dac_init(i2c0);
    dac_interrupt_init();
    printf("DAC intialized.\r\n");

    printf("Audio init complete.\r\n");

    // Initialize buttons with a 50ms scan rate
    buttons_init(50);
    printf("\r\nButtons intializedr\n");

    pot_init();
    printf("\r\npot intialized\r\n");
    
    // --- Manually Initialize SPI1 for VS1053 Codec ---
    // 1. Initialize the hardware peripheral to your desired operational speed
    uint actual_baud = spi_init(spi1, 1 * 1000 * 1000); // 1 MHz
    printf("SPI1 initialized for VS1053 at %u Hz\r\n", actual_baud);

    // 2. Configure the GPIO pins to use the SPI hardware function
    gpio_set_function(30, GPIO_FUNC_SPI); // SCK
    gpio_set_function(31, GPIO_FUNC_SPI); // MOSI
    gpio_set_function(28, GPIO_FUNC_SPI); // MISO

    // 3. Configure the Slave Select / Chip Select (CS) pin
    // Note: The VS1053 usually has an XCS (Command CS) and an XDCS (Data CS).
    // Ensure whichever pin was mapped to 'ss_gpio' (GPIO 2) is explicitly configured as an output.
    gpio_init(32);
    gpio_init(33);
    gpio_set_dir(32, GPIO_OUT);
    gpio_set_dir(33, GPIO_OUT);
    gpio_put(32, 1); // Deselect by default (high)
    gpio_put(33, 1); // Deselect by default (high)
    printf("SPI1 GPIO pins assigned.\r\n");

    vs1053_init(player);
    printf("VS1053 initialized.\r\n");
    
    vs1053_set_volume(player, 0x01, 0x01); 
    printf("VS1053 volume set to max!\r\n");

    // Enable I2S output
    vs1053_enable_i2s(player);
    printf("VS1053 I2S enabled.\r\n");

    multicore_reset_core1();
    sleep_ms(10);
    multicore_launch_core1(core1_entry);
    printf("CORE 1 LAUNCHED!\r\n");
    
    printf("\r\nFinished sb_hw_init\r\n");
}
