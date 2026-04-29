#include "lib/sb_util/global_vars.h"
#include "lib/sb_util/sb_util.h"

/* Text Display Stuff */
mutex_t text_buff_mtx;
semaphore_t text_sem;


char text_buff_temp[120];
struct Node *head = NULL;
int visualizer = 5;

uint8_t marquee_title_start = 0;
uint8_t marquee_artist_start = 0;
uint8_t marquee_album_start = 0;

void set_visualizer(int num)
{
    visualizer = num;
}

void app_node(char *str)
{
    if (sem_available(&text_sem) >= 10)
    {
        return;
    }
    mutex_enter_blocking(&text_buff_mtx);

    struct Node *n = calloc(1, sizeof(struct Node));
    if (n == NULL)
    {
        printf("Error using calloc in app_node, freeing LL!");
        n = head;
        struct Node *prev = head;
        while (n != NULL)
        {
            prev = n;
            n = n->next;
            free(prev);
        }
        mutex_exit(&text_buff_mtx);
        // sleep_ms(100000);
        return;
    }

    n->next = NULL;
    strncpy(n->str, str, sizeof(n->str));

    if (head == NULL)
    {
        head = n;
    }
    else
    {
        struct Node *prev = head;
        while (prev->next != NULL)
        {
            prev = prev->next;
        }
        prev->next = n;
    }
    mutex_exit(&text_buff_mtx);
    sem_release(&text_sem);
    return;
}

void dprint(char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(text_buff_temp, fmt, args);
    va_end(args);
    app_node(text_buff_temp);
#ifdef DEBUG
    printf("dprint: \'%s\' | strlen:%d sem_avail:%d\r\n", text_buff_temp, strlen(text_buff_temp), sem_available(&text_sem));
#endif
    return;
}

// This is the main loop for Core 1

int start;
void core1_entry()
{
    while (1)
    {
        switch (visualizer)
        {
        case 0: // Album Art
            if (album_art_ready)
            {
                // Draw art once
                album_art_centered();
                st7789_set_cursor(0, 0);
                st7789_ramwr();
                spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                spi_write16_blocking(spi0, frame_buffer, 240 * 240);

                // Lock into an LED-only
                while (visualizer == 0)
                {
                    adc_select_input(ADC_CH_L);
                    uint16_t raw_l = adc_read();

                    adc_select_input(ADC_CH_R);
                    uint16_t raw_r = adc_read();

                    pca9685_update_vu(&vu_meter, raw_l, raw_r);

                    addIcons(frame_buffer, enableIcons);
                    st7789_set_cursor(0, 0);
                    st7789_ramwr();
                    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                    spi_write16_blocking(spi0, frame_buffer, 240 * 240);
                    // sleep_ms(16); // Throttle to ~60FPS
                }
            }
            break;

        case 1: // Oscilloscope
            update_scope_core1();
            break;

        case 2: // FFT
            process_audio_batch();

            memset(frame_buffer, 0, sizeof(frame_buffer));
            draw_bins(60);

            //Place pause Icon on screen
            addIcons(frame_buffer, enableIcons);
            st7789_set_cursor(0, 0);
            st7789_ramwr();
            spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            break;

        case 3: // Lissajous
            process_audio_batch();
            draw_lissajous();
            break;

        case 4: // Lissajous connected
            process_audio_batch();
            draw_lissajous_connected();
            break;

        case 5:
            if (sem_acquire_timeout_ms(&text_sem, 10)) {
                printf(" core1: aquired lock\r\n");

                memmove(&frame_buffer, &frame_buffer[SCREEN_WIDTH * (font_height)], sizeof(uint16_t) * (SCREEN_WIDTH) * (SCREEN_HEIGHT - font_height));
                memset(&frame_buffer[SCREEN_WIDTH * (SCREEN_HEIGHT - font_height)], 0, sizeof(uint16_t) * (SCREEN_WIDTH) * (font_height));
                mutex_enter_blocking(&text_buff_mtx);

                if (head == NULL) {
                    printf("Err! Core 1 head is NULL");
                    mutex_exit(&text_buff_mtx);
                    continue;
                }
                printf("core 1: %s | %d\r\n", head->str, strlen(text_buff_temp));
                st7789_draw_string(1, SCREEN_HEIGHT - font_height - 5, head->str, WHITE);
                struct Node *n = head;
                head = head->next;
                if (n != NULL)
                {
                    free(n);
                }
                mutex_exit(&text_buff_mtx);
                st7789_set_cursor(0, 0);
                st7789_ramwr();
                spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
                spi_write16_blocking(spi0, frame_buffer, 240 * 240);
                // sleep_ms(1000);
                printf(" core 1 finished print\r\n");
            }
            break;
        
        case 6:
            clear_framebuffer();
            start =  (song_choice < 6) ? 0 : song_choice - 5;
            track_info_t *track;
            track_info_t *selected_track;
            char buf[256]; // buffer for string to write to display
            char marquee_title[32]; // buffer for scrolling title marquee
            char md_artist[128]; // artist metadata of currently selected track
            char md_album[128]; // album metadata of currently selected track
            char marquee_artist[32]; // buffer for scrolling album marquee
            char marquee_album[32]; // // buffer for scrolling album marquee
            uint16_t marquee_delay = 1000;

            static uint32_t marquee_delay_start_ms = 0;
            static int last_song_choice = -1;


            for (int i = 0; i<10; i++){
                if (start + i >= count) {
                    break;
                }
                track = &tracks[start+i];
                selected_track = &tracks[song_choice];
                sprintf(buf, "%d", start+i+1); //Index at 1 for users
                strcat(buf, " ");
                if (start + i == song_choice) {
                    strcat(buf, marquee_title);
                    st7789_draw_string(1, 0 + i * font_height, buf, HIGHLIGHT_COLOR_SECONDARY);
                }
                else{
                    strcat(buf, track->title);
                    st7789_draw_string(1, 0 + i * font_height, buf, WHITE);
                }
            }

            if (song_choice != last_song_choice) {
                marquee_title_start = 0;
                marquee_artist_start = 0;
                marquee_album_start = 0;

                marquee_delay_start_ms = to_ms_since_boot(get_absolute_time());

                last_song_choice = song_choice;
            }

            /* ##### MARQUEE BLOCK - WRITTEN BY ERIC ##### */

            // 'static' ensures this variable survives between function calls
            static uint32_t last_marquee_update_ms = 0;
            // Get the current time since the chip started
            uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());

            // crude counter to update marquee
            // Here, we update the window values for all three marquees every 100 milliseconds.
            if (current_time_ms - last_marquee_update_ms >= 100) {
                if (current_time_ms - marquee_delay_start_ms < marquee_delay) {
                    last_marquee_update_ms = current_time_ms;
                } else { if (strlen(selected_track->artist) > 20) {
                        // only apply marquee effect if album name is greater than window
                        marquee_artist_start++; // increment marquee pointer
                        if (marquee_artist_start > strlen(selected_track->artist) + 8) {  // set limit to virtual length of 28 (window size + number of spaces)
                            marquee_artist_start = 0; // reset only when marquee pointer goes over virtual length
                        }
                    } else {
                        marquee_artist_start = 0;
                    }

                    if (strlen(selected_track->album) > 20) {
                        // only apply marquee effect if album name is greater than window
                        marquee_album_start++; // increment marquee pointer
                        if (marquee_album_start > strlen(selected_track->album) + 8) {  // set limit to virtual length of 28 (window size + number of spaces)
                            marquee_album_start = 0; // reset only when marquee pointer goes over virtual length
                        }
                    } else {
                        marquee_album_start = 0;
                    }

                    if (strlen(selected_track->title) > 18) {
                        // only apply marquee effect if album name is greater than window
                        marquee_title_start++; // increment marquee pointer
                        if (marquee_title_start > strlen(selected_track->title) + 6) {  // set limit to virtual length of 28 (window size + number of spaces)
                            marquee_title_start = 0; // reset only when marquee pointer goes over virtual length
                        }
                    } else {
                        marquee_title_start = 0;
                    }

                    last_marquee_update_ms = current_time_ms;
                }
            }

     
            // Now that we've updated the marquee windows, splice the strings together for smooth scrolling
            uint8_t albumLen = strlen(selected_track->album);
            uint8_t artistLen = strlen(selected_track->artist);
            uint8_t titleLen = strlen(selected_track->title);
            uint8_t window = 20;
            uint8_t gap = 8;
            uint8_t virtualWindow = 28;

            if (titleLen > 18) {
                if (titleLen - marquee_title_start >= 18) {
                    memcpy(marquee_title, selected_track->title + marquee_title_start, 18);
                } else if (titleLen - marquee_title_start > 0) {
                    memcpy(marquee_title, selected_track->title + marquee_title_start, titleLen - marquee_title_start);
                    uint8_t numSpaces = 6;
                    if ((titleLen - marquee_title_start) + numSpaces > 18) {
                        numSpaces = 18 - (titleLen - marquee_title_start);
                    }
                    for (int i=0; i<numSpaces; i++) {
                        marquee_title[titleLen - marquee_title_start + i] = ' ';
                    }
                    memcpy(marquee_title + (titleLen - marquee_title_start) + numSpaces, selected_track-> title, 18 - ((titleLen - marquee_title_start) + numSpaces));
                } else {
                    int numSpaces = 6 - (marquee_title_start - titleLen);
                    if (numSpaces < 0) numSpaces = 0;
                    if (numSpaces > window) numSpaces = 18;
                    for (int i = 0; i < numSpaces; i++) {
                        marquee_title[i] = ' ';
                    }
                    memcpy(marquee_title + numSpaces,
                        selected_track->title,
                        18 - numSpaces);
                    marquee_title[18] = '\0';
                }
            } else {
                memcpy(marquee_title, selected_track->title, 18);
            }

            if (albumLen > 20) {
                if (albumLen - marquee_album_start >= window) {
                    // if there are more characters left than the marquee window, continue as necessary
                    memcpy(marquee_album, selected_track->album + marquee_album_start, 20);
                } else if (albumLen - marquee_album_start > 0) {
                    // if there are less characters than the window size, splice string into two
                    // whatever's left, plus some spaces, then the start of the string

                    // Whatever's left at the end of string
                    memcpy(marquee_album, selected_track->album + marquee_album_start, albumLen - marquee_album_start);

                    // Now, tack on eight spaces
                    uint8_t numSpaces = gap;
                    if ((albumLen - marquee_album_start) + numSpaces > window) {
                        numSpaces = window - (albumLen - marquee_album_start);
                    }
                    for (int i=0; i<numSpaces; i++) {
                        marquee_album[albumLen - marquee_album_start + i] = ' ';
                    }

                    // Now, tack on however many characters we can from the beginning of string
                    memcpy(marquee_album + (albumLen - marquee_album_start) + numSpaces, selected_track-> album, window - ((albumLen - marquee_album_start) + numSpaces));
                } else {
                    // Now, all text of the marquee has passed. All that remains are 8 spaces and the begnning of the text tacked behind.
                    // So we decrement the number of spaces in the front, and drag in the beginning of the marquee text.
                    // To do this, we need a counter to track the spaces. We will use the marquee pointer for this.
                    // At this point, the marquee pointer is 20 to 27. I think.

                    // uint8_t numSpaces = (20 + 8) - marquee_album_start; // this way, we get 8 to 1 spaces
                    uint8_t numSpaces = gap - (marquee_album_start - albumLen);

                    if (numSpaces > window) numSpaces = window;

                    // Fill leading spaces
                    for (int i = 0; i < numSpaces; i++) {
                        marquee_album[i] = ' ';
                    }

                    // Fill remaining with album text
                    memcpy(marquee_album + numSpaces,
                        selected_track->album,
                        window - numSpaces);

                    marquee_album[window] = '\0';
                }
            } else {
                memcpy(marquee_album, selected_track->album, 20);
            }

            if (artistLen > 20) {
                if (artistLen - marquee_artist_start >= window) {
                    // if there are more characters left than the marquee window, continue as necessary
                    memcpy(marquee_artist, selected_track->artist + marquee_artist_start, 20);
                } else if (artistLen - marquee_artist_start > 0) {
                    // if there are less characters than the window size, splice string into two
                    // whatever's left, plus some spaces, then the start of the string

                    // Whatever's left at the end of string
                    memcpy(marquee_artist, selected_track->artist + marquee_artist_start, artistLen - marquee_artist_start);

                    // Now, tack on eight spaces
                    uint8_t numSpaces = gap;
                    if ((artistLen - marquee_artist_start) + numSpaces > window) {
                        numSpaces = window - (artistLen - marquee_artist_start);
                    }
                    for (int i=0; i<numSpaces; i++) {
                        marquee_artist[artistLen - marquee_artist_start + i] = ' ';
                    }

                    // Now, tack on however many characters we can from the beginning of string
                    memcpy(marquee_artist + (artistLen - marquee_artist_start) + numSpaces, selected_track-> artist, window - ((artistLen - marquee_artist_start) + numSpaces));
                } else {
                    // Now, all text of the marquee has passed. All that remains are 8 spaces and the begnning of the text tacked behind.
                    // So we decrement the number of spaces in the front, and drag in the beginning of the marquee text.
                    // To do this, we need a counter to track the spaces. We will use the marquee pointer for this.
                    // At this point, the marquee pointer is 20 to 27. I think.

                    // uint8_t numSpaces = (20 + 8) - marquee_artist_start; // this way, we get 8 to 1 spaces
                    uint8_t numSpaces = gap - (marquee_artist_start - artistLen);

                    if (numSpaces > window) numSpaces = window;

                    // Fill leading spaces
                    for (int i = 0; i < numSpaces; i++) {
                        marquee_artist[i] = ' ';
                    }

                    // Fill remaining with artist text
                    memcpy(marquee_artist + numSpaces,
                        selected_track->artist,
                        window - numSpaces);

                    marquee_artist[window] = '\0';
                }
            } else {
                memcpy(marquee_artist, selected_track->artist, 20);
            }

            sprintf(md_artist, "%s", marquee_artist);
            sprintf(md_album, "%s", marquee_album);
            st7789_draw_string(1, -2 + 10 * font_height, md_artist, HIGHLIGHT_COLOR_PRIMARY);
            st7789_draw_string(1, -2 + 11 * font_height, md_album, HIGHLIGHT_COLOR_PRIMARY);

            st7789_set_cursor(0, 0);
            st7789_ramwr();
            spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            break;

        case 7:
            clear_framebuffer();
            start = count-10>0 ? count-10 : 0;
            for (int i = 0; i<10; i++){
                if (start + i >= count){
                    break;
                }
                track_info_t *track = &tracks[start+i];
                char buf[256];
                sprintf(buf, "%d", start+i+1); //Index at 1 for users
                strcat(buf, " ");
                strcat(buf, track->title);
                if (start + i == song_choice){
                    st7789_draw_string(1, 5 + i * font_height, buf, HIGHLIGHT_COLOR_PRIMARY);
                }
                else{
                    st7789_draw_string(1, 5 + i * font_height, buf, WHITE);
                }
            }
            st7789_set_cursor(0, 0);
            st7789_ramwr();
            spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            break;

        default:
            visualizer = 0;
            break;
        }
    }
}


/* =========================================================
   COPY YOUR EXISTING FUNCTIONS BELOW
   (unchanged, just made static)
   ========================================================= */

void update_scope_core1()
{
    static int x = 0;
    static int last_y_l = OFFSET_L;
    static int last_y_r = OFFSET_R;
    static int led_throttle = 0;

    // 1. Sample Channels
    adc_select_input(ADC_CH_L);
    uint16_t raw_l = adc_read();
    adc_select_input(ADC_CH_R);
    uint16_t raw_r = adc_read();
    
    // 2. Map to Split Offsets
    // Left Channel centered at 150
    int dev_l = (int)raw_l - ADC_BIAS_CENTER;
    int y_l = OFFSET_L - (dev_l * TARGET_HEIGHT / ADC_RANGE_PKPK);

    // Right Channel centered at 90
    int dev_r = (int)raw_r - ADC_BIAS_CENTER;
    int y_r = OFFSET_R - (dev_r * TARGET_HEIGHT / ADC_RANGE_PKPK);

    // 3. Clamps (Keep them within their respective zones or full screen)
    if (y_l < 0)
        y_l = 0;
    if (y_l > 239)
        y_l = 239;
    if (y_r < 0)
        y_r = 0;
    if (y_r > 239)
        y_r = 239;

    // 4. Clear Column
    for (int i = 0; i < 240; i++)
    {
        frame_buffer[i * 240 + x] = BG_COLOR;
    }

    // 5. Draw Left (Green)
    int start_l = (y_l < last_y_l) ? y_l : last_y_l;
    int end_l = (y_l < last_y_l) ? last_y_l : y_l;
    for (int i = start_l; i <= end_l; i++)
    {
        frame_buffer[i * 240 + x] |= WAVE_L_COLOR;
    }

    // 6. Draw Right (Cyan)
    int start_r = (y_r < last_y_r) ? y_r : last_y_r;
    int end_r = (y_r < last_y_r) ? last_y_r : y_r;
    for (int i = start_r; i <= end_r; i++)
    {
        frame_buffer[i * 240 + x] |= WAVE_R_COLOR;
    }

    last_y_l = y_l;
    last_y_r = y_r;
    x++;

    // 7. Push to Display
    if (x >= 240)
    {
        addIcons(frame_buffer, enableIcons);
        x = 0;
        st7789_set_cursor(0, 0);
        st7789_ramwr();
        spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        spi_write16_blocking(spi0, frame_buffer, 240 * 240);
        pca9685_update_vu(&vu_meter, raw_l, raw_r);
    }
}

// Helper function to sample audio and update the LEDs
static void process_audio_batch()
{
    static int history_ptr = 0;
    uint16_t max_dev_l = 0;
    uint16_t max_dev_r = 0;

    for (int i = 0; i < 32; i++)
    {
        // Read Left ONCE
        adc_select_input(ADC_CH_L);
        uint16_t raw_l = adc_read();
        audio_history_l[history_ptr] = (cplx)raw_l;

        // Read Right ONCE
        adc_select_input(ADC_CH_R);
        uint16_t raw_r = adc_read();
        audio_history_r[history_ptr] = (cplx)raw_r;

        // calculate absolute deviation from the DC bias center //find a way to remove this, subtract
        uint16_t dev_l = abs((int)raw_l - ADC_BIAS_CENTER);
        uint16_t dev_r = abs((int)raw_r - ADC_BIAS_CENTER);

        // Onny take max value in each batch
        if (dev_l > max_dev_l)
            max_dev_l = dev_l;
        if (dev_r > max_dev_r)
            max_dev_r = dev_r;

        history_ptr = (history_ptr + 1) % HISTORY_SIZE;
        sleep_us(10);
    }

    // Re-add the bias so the VU meter math processes the peak correctly
    pca9685_update_vu(&vu_meter, ADC_BIAS_CENTER + max_dev_l, ADC_BIAS_CENTER + max_dev_r);
}

#define CENTER_Y 20
#define MARGIN_LEFT 200
#define BAR_WIDTH 4
#define GAP_PX 2

//Adds icons and samples ADC
void addIcons(uint16_t* frame_buffer, bool enabled){
    adc_select_input(POT_CH);
    potVal = adc_read();
    if (enabled){

        //Place pause Icon on screen
        for (int y = 0; y < 20; y++)
        {
            uint16_t *dst = &frame_buffer[y * SCREEN_WIDTH];
            uint16_t *src = &playStatus[y * 20];
            memcpy(dst, src, 20 * sizeof(uint16_t));
        }
        //Place rewind/fastforward Icon on screen (Starts at x = 24)
        int icon_spacing = 34; // 20px icon width + 4px gap
        for (int y = 0; y < 20; y++)
        {
            uint16_t *dst = &frame_buffer[(y * SCREEN_WIDTH) + icon_spacing];
            uint16_t *src = &ff_rew_status[y * 20];
            memcpy(dst, src, 20 * sizeof(uint16_t));
        }
        //progress bar
        for (int y = 235; y < 240; y++)
        {
            for (int x = 0; x < 240; x++)
            {
                if (x < progress_bar)
                {
                    frame_buffer[y * 240 + x] = played_progres_color; // Played part
                }
                else
                {
                    frame_buffer[y * 240 + x] = background_progress_color; // Remaining part
                }
            }
        }

        //eq Icons
        for (int i = 0; i < 6; i++){
            float eqGain = dac_eq_get_gain(i);
            
            // Map the gain to pixel height
            int pixel_height = (int)((eqGain / MAX_GAIN_DB) * 20.0f);

            // Determine vertical start and end points based on positive/negative gain
            int y_start, y_end;

            if (pixel_height >= 0)
            {
                // Positive gain: bar goes UP from center (subtracting from Y)
                y_start = CENTER_Y - pixel_height;
                y_end = CENTER_Y;
            }
            else
            {
                // Negative gain: bar goes DOWN from center (adding to Y)
                y_start = CENTER_Y;
                y_end = CENTER_Y - pixel_height; // pixel_height is negative, so subtracting it ADDS to Y
            }

            // Determine horizontal bounds for this specific bar
            int x_start = MARGIN_LEFT + i * (BAR_WIDTH + GAP_PX);
            int x_end = x_start + BAR_WIDTH;

            // 3. Draw the white block directly into the frame buffer
            for (int y = y_start; y <= y_end; y++)
            {
                for (int x = x_start; x < x_end; x++)
                {
                    // Assuming WHITE is defined as 0xFFFF
                    if (get_selected_band() == i) frame_buffer[y * SCREEN_WIDTH + x] = 0x001F;
                    else frame_buffer[y * SCREEN_WIDTH + x] = 0xFFFF; 
                }
            }
        }
    }
}