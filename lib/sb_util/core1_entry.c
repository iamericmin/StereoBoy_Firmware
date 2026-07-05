#include "global_vars.h"
#include "sb_util.h"

/* Text Display Stuff */
mutex_t text_buff_mtx;
semaphore_t text_sem;


char text_buff_temp[120];
struct Node *head = NULL;
int visualizer = 1;

uint8_t marquee_title_start = 0;
uint8_t marquee_artist_start = 0;
uint8_t marquee_album_start = 0;

// In your main C file (global scope)
// track_info_t runtime_playing_track; 
// track_info_t *current_track = &runtime_playing_track;

int count;

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

void music_menu() {
    
}

int start;
void core1_entry()
{
    while (1)
    {
        adc_select_input(POT_CH);
        potVal = adc_read();
        switch (visualizer) {
        case 0: // Album Art
            // track = &tracks[song_choice];
            // uint64_t hash = generate_FNV(track->title, track->artist, track->album);
            // uint32_t pointer = lookup_LUT(hash);
            // st7789_set_cursor(0, 0);
            // st7789_ramwr();
            // spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            // spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            // Lock into an LED-only
            process_audio_batch();
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
            break;
        case 1: // Oscilloscope
            update_scope_core1();
            break;

        // case 2: // FFT
        //     process_audio_batch();

        //     memset(frame_buffer, 0, sizeof(frame_buffer));
        //     draw_bins(60);

        //     //Place pause Icon on screen
        //     addIcons(frame_buffer, enableIcons);
        //     st7789_set_cursor(0, 0);
        //     st7789_ramwr();
        //     spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        //     spi_write16_blocking(spi0, frame_buffer, 240 * 240);
        //     break;

        // case 3: // Lissajous
        //     process_audio_batch();
        //     draw_lissajous();
        //     break;

        // case 4: // Lissajous connected
        //     process_audio_batch();
        //     draw_lissajous_connected();
        //     break;

        // case 5:
            // if (sem_acquire_timeout_ms(&text_sem, 10)) {
            //     printf(" core1: aquired lock\r\n");

            //     memmove(&frame_buffer, &frame_buffer[SCREEN_WIDTH * (font_height)], sizeof(uint16_t) * (SCREEN_WIDTH) * (SCREEN_HEIGHT - font_height));
            //     memset(&frame_buffer[SCREEN_WIDTH * (SCREEN_HEIGHT - font_height)], 0, sizeof(uint16_t) * (SCREEN_WIDTH) * (font_height));
            //     mutex_enter_blocking(&text_buff_mtx);

            //     if (head == NULL) {
            //         printf("Err! Core 1 head is NULL");
            //         mutex_exit(&text_buff_mtx);
            //         continue;
            //     }

            //     printf("core 1: %s | %d\r\n", head->str, strlen(text_buff_temp));
            //     st7789_draw_string(1, SCREEN_HEIGHT - font_height - 5, head->str, WHITE);
            //     struct Node *n = head;
            //     head = head->next;
            //     if (n != NULL)
            //     {
            //         free(n);
            //     }
            //     mutex_exit(&text_buff_mtx);
            //     st7789_set_cursor(0, 0);
            //     st7789_ramwr();
            //     spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            //     spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            //     // sleep_ms(1000);
            //     printf(" core 1 finished print\r\n");
            // }
            // break;
        
        case 6:
            process_audio_batch();
            clear_framebuffer();
            
            // FIX: The active song is always pinned to the middle row, which is index 5 in our cache layout
            track_info_t *selected_track = &track_window[5]; 
            
            static char buf[256];          // Render row string buffer
            static char marquee_title[32];  // Safe isolated scroll windows
            static char marquee_artist[32];
            static char marquee_album[32];
            
            uint16_t marquee_delay = 1000;
            static uint32_t marquee_delay_start_ms = 0;
            static int last_song_choice = -1;

            // Handle track change state resets
            if (song_choice != last_song_choice) {
                marquee_title_start = 0;
                marquee_artist_start = 0;
                marquee_album_start = 0;
                marquee_delay_start_ms = to_ms_since_boot(get_absolute_time());
                last_song_choice = song_choice;
            }

            /* ─── MARQUEE TIME ENGINE (UNTOUCHED) ─── */
            static uint32_t last_marquee_update_ms = 0;
            uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());

            if (current_time_ms - last_marquee_update_ms >= 100) {
                if (current_time_ms - marquee_delay_start_ms >= marquee_delay) {
                    if (strlen(selected_track->title) > 18) {
                        marquee_title_start++;
                        if (marquee_title_start > strlen(selected_track->title) + 6) marquee_title_start = 0;
                    } else { marquee_title_start = 0; }

                    if (strlen(selected_track->artist) > 20) {
                        marquee_artist_start++;
                        if (marquee_artist_start > strlen(selected_track->artist) + 8) marquee_artist_start = 0;
                    } else { marquee_artist_start = 0; }

                    if (strlen(selected_track->album) > 20) {
                        marquee_album_start++;
                        if (marquee_album_start > strlen(selected_track->album) + 8) marquee_album_start = 0;
                    } else { marquee_album_start = 0; }
                }
                last_marquee_update_ms = current_time_ms;
            }

            /* ─── VIRTUAL MARQUEE GENERATION (UNTOUCHED) ─── */
            uint8_t titleLen  = strlen(selected_track->title);
            uint8_t artistLen = strlen(selected_track->artist);
            uint8_t albumLen  = strlen(selected_track->album);

            if (titleLen > 18) {
                for (int m = 0; m < 18; m++) {
                    uint32_t virt_idx = (marquee_title_start + m) % (titleLen + 6);
                    marquee_title[m] = (virt_idx < titleLen) ? selected_track->title[virt_idx] : ' ';
                }
                marquee_title[18] = '\0';
            } else { snprintf(marquee_title, 19, "%s", selected_track->title); }

            if (artistLen > 20) {
                for (int m = 0; m < 20; m++) {
                    uint32_t virt_idx = (marquee_artist_start + m) % (artistLen + 8);
                    marquee_artist[m] = (virt_idx < artistLen) ? selected_track->artist[virt_idx] : ' ';
                }
                marquee_artist[20] = '\0';
            } else { snprintf(marquee_artist, 21, "%s", selected_track->artist); }

            if (albumLen > 20) {
                for (int m = 0; m < 20; m++) {
                    uint32_t virt_idx = (marquee_album_start + m) % (albumLen + 8);
                    marquee_album[m] = (virt_idx < albumLen) ? selected_track->album[virt_idx] : ' ';
                }
                marquee_album[20] = '\0';
            } else { snprintf(marquee_album, 21, "%s", selected_track->album); }

            /* ─── LOCK-TO-CENTER LIST RENDER PANEL ─── */
            // Loop exactly 10 times to draw our rows (5 above, 1 selected, 4 below)
            for (int i = 0; i < 10; i++) {
                // Calculate the real absolute global index for the current row line
                int current_abs_idx = (int)song_choice - 5 + i;

                // Bounds-check each line item individually
                if (current_abs_idx >= 0 && current_abs_idx < count) {
                    // Populate index number prefix (1-indexed for the user)
                    snprintf(buf, sizeof(buf), "%d ", current_abs_idx + 1);

                    // Row index 5 is always the exact vertical center line
                    if (i == 5) {
                        strncat(buf, marquee_title, sizeof(buf) - strlen(buf) - 1);
                        st7789_draw_string(1, 0 + i * font_height, buf, HIGHLIGHT_COLOR_SECONDARY);
                    } 
                    else {
                        // The track window maps directly to screen line index 'i'
                        strncat(buf, track_window[i].title, sizeof(buf) - strlen(buf) - 1);
                        st7789_draw_string(1, 0 + i * font_height, buf, WHITE);
                    }
                } 
                else {
                    // If the current row falls out of playlist limits (e.g. tracks before track #1),
                    // render an empty line string so old text buffer data doesn't pool on screen.
                    st7789_draw_string(1, 0 + i * font_height, " ", WHITE);
                }
            }

            // Render bottom metadata sub-panels
            st7789_draw_string(1, -2 + 10 * font_height, marquee_artist, HIGHLIGHT_COLOR_PRIMARY);
            st7789_draw_string(1, -2 + 11 * font_height, marquee_album, HIGHLIGHT_COLOR_PRIMARY);

            // Blit frame buffer contents to display
            st7789_set_cursor(0, 0);
            st7789_ramwr();
            spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            break;

        case 7:
            clear_framebuffer();

            for (int i = 0; i < 11; i++) {
                track_info_t *track = &track_window[i];
                
                // Check if this slot is empty (out of bounds padding from our earlier function)
                if (track->filename[0] == '\0') {
                    continue; 
                }

                // Calculate the absolute track number for the UI text (1-indexed)
                // 'song_choice' matches track_window[5]. So index 'i' is offset by (i - 5).
                int32_t absolute_track_num = (int32_t)song_choice + (i - 5) + 1;

                char buf[256];
                sprintf(buf, "%d %s", absolute_track_num, track->title); // Clean combination of index and title

                // If i == 5, this is the currently selected track (the middle of our window)
                if (i == 5) {
                    st7789_draw_string(1, 5 + i * font_height, buf, HIGHLIGHT_COLOR_PRIMARY);
                } else {
                    st7789_draw_string(1, 5 + i * font_height, buf, WHITE);
                }
            }

            st7789_set_cursor(0, 0);
            st7789_ramwr();
            spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
            spi_write16_blocking(spi0, frame_buffer, 240 * 240);
            break;

        default:
            visualizer = (visualizer == 2 || 3 || 4 || 5) ? 6 : 0;
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
        process_audio_batch();
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
    // adc_select_input(POT_CH);
    // potVal = adc_read();
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