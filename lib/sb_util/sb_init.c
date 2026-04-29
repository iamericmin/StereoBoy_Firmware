#include "global_vars.h"

#include "lib/sb_util/sb_util.h"

static FATFS fs;

static int dma_chan = -1;
static dma_channel_config dcc;

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
    sleep_ms(100);
    
    // SWRESET (01h): Software Reset
    st7789_cmd(0x01, NULL, 0);
    sleep_ms(150);

    // SLPOUT (11h): Sleep Out
    st7789_cmd(0x11, NULL, 0);
    sleep_ms(50);

    // COLMOD (3Ah): Interface Pixel Format
    // - RGB interface color format     = 65K of RGB interface
    // - Control interface color format = 16bit/pixel
    st7789_cmd(0x3a, (uint8_t[]){ 0x55 }, 1);
    sleep_ms(10);

    // MADCTL (36h): Memory Data Access Control
    // - Page Address Order            = Top to Bottom
    // - Column Address Order          = Left to Right
    // - Page/Column Order             = Normal Mode
    // - Line Address Order            = LCD Refresh Top to Bottom
    // - RGB/BGR Order                 = RGB
    // - Display Data Latch Data Order = LCD Refresh Left to Right
    st7789_cmd(0x36, (uint8_t[]){ 0xC0 }, 1);
   
    st7789_caset(0, width);
    st7789_raset(0, height);

    // INVON (21h): Display Inversion On
    st7789_cmd(0x21, NULL, 0);
    sleep_ms(10);

    // NORON (13h): Normal Display Mode On
    st7789_cmd(0x13, NULL, 0);
    sleep_ms(10);

    // DISPON (29h): Display On
    st7789_cmd(0x29, NULL, 0);
    sleep_ms(10);

    gpio_put(st7789_cfg.gpio_bl, 1);
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

    multicore_launch_core1(core1_entry);
    printf("CORE 1 LAUNCHED!\r\n");
}

int sb_scan_tracks(track_info_t *tracks, int max_tracks)
{
    DIR dir;
    FILINFO fno;
    count = 0;

    f_opendir(&dir, "0:/");

    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if (fno.fattrib & AM_DIR)
            continue;

        char *ext = strrchr(fno.fname, '.');
        if (ext && !strcasecmp(ext, ".mp3") && count < MAX_TRACKS)
        {
            get_mp3_metadata(fno.fname, &tracks[count]);
            count++;
            // dprint("Read song %d", count);
        }
    }

    f_closedir(&dir);

    if (count == 0)
    {
        printf("No MP3 files found.\r\n");
        while (1)
            ;
    }

    qsort(tracks, count, sizeof(track_info_t), compare_filenames);

    return count;
}

void sb_hw_init(vs1053_t *player, st7789_t *display)
{

    mutex_init(&text_buff_mtx);
    sem_init(&text_sem, 0, 255);

    // set SPI1 for codec and SD card
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    // set I2C0 for DAC at 400KHz
    gpio_set_function(PIN_I2C0_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C0_SDA, GPIO_FUNC_I2C);
    i2c_init(i2c0, 400 * 1000);
    // gpio_pull_up(PIN_I2C0_SCL);
    // gpio_pull_up(PIN_I2C0_SDA);
    dprint("SPI0 and I2C0 initialized.");
    printf("SPI0 and I2C0 initialized.\r\n");

    // set I2C1 for PCA9685 at 400KHz
    gpio_set_function(PIN_I2C1_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C1_SCL, GPIO_FUNC_I2C);
    i2c_init(i2c1, 400 * 1000);
    // gpio_pull_up(PIN_I2C1_SDA);
    // gpio_pull_up(PIN_I2C1_SCL);
    printf("I2C1 initialized.\r\n");

    // LED driver init
    if (pca9685_init(&vu_meter, i2c1, 0x40))
    {
        printf("PCA9685 LED Driver initialized!\r\n");
    }
    else
    {
        printf("WARNING: PCA9685 Init Failed!\r\n");
    }

    if (!sd_init_driver())
    {
        while (1)
        {
            dprint("SD init failed");
            printf("SD init failed\r\n");
        }
    }
    else
    {
        dprint("SD card initialized!");
        printf("SD card initialized!\r\n");
    }

    FRESULT fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK)
    {
        while (1)
        {
            dprint("SD Mount failed: %d", fr);
            printf("SD Mount failed: %d\n", fr);
        }
    }
    else
    {
        dprint("SD card mounted!");
        printf("SD card mounted!\r\n");
    }

    adc_init();        // Inside sb_hw_init
    adc_gpio_init(46); // Left
    adc_gpio_init(45); // Right

    printf("Oscope ADC initialized!\r\n");
    dprint("Oscope ADC initialized!");

    sleep_ms(100); // seems to help flaky display issues

    sb_display_init(display);
    printf("test point 1");

    vs1053_init(player);
    printf("test point 2");

    printf("VS1053 initialized.\r\n");
    dprint("VS1053 initialized.");
    vs1053_set_volume(player, 0x01, 0x01); // chnged from 0 (0x00) to -12dB (0x0202) to -6dB (0x0101)
    printf("VS1053 volume set to max!\r\n");
    dprint("VS1053 volume set to max!");

    // Enable I2S output
    vs1053_enable_i2s(player);
    printf("VS1053 I2S enabled.\r\n");
    dprint("VS1053 I2S enabled.");

    // initialize DAC
    dac_init(i2c0);
    dac_interrupt_init();
    printf("DAC intialized.\r\n");
    dprint("DAC intialized.");

    printf("Audio init complete.\r\n");
    dprint("Audio init complete.");

    // Initialize buttons with a 10ms scan rate
    buttons_init(10);
    printf("\r\nButtons intializedr\n");

    pot_init();
    printf("\r\npot intialized\r\n");

    dprint("Finished sb_hw_init");
    printf("\r\nFinished sb_hw_init\r\n");

    // pca9685_sleep(&vu_meter, 1);
}
