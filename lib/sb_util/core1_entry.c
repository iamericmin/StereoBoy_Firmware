#include "global_vars.h"
#include "sb_util.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "pico/time.h"

#include <string.h>
#include <stdio.h>

/* Text Display Stuff */
mutex_t text_buff_mtx;
semaphore_t text_sem;

char text_buff_temp[120];
struct Node *head = NULL;
int visualizer = 1;

uint8_t marquee_title_start = 0;
uint8_t marquee_artist_start = 0;
uint8_t marquee_album_start = 0;

uint8_t marquee_scroll_rate = 100;
bool home_marquee_dir = false;

cplx audio_history_l[HISTORY_SIZE];
cplx audio_history_r[HISTORY_SIZE];

#define LED_R 24
#define LED_G 25
#define LED_B 26

static bool repeating_timer_callback(struct repeating_timer *t) {
    gpio_put(LED_R, !gpio_get(LED_R)); 
    // gpio_put(LED_G, !gpio_get(LED_G));
    // gpio_put(LED_B, !gpio_get(LED_B));
 
    return true;
}

static void marquee_plus(uint8_t *scroll_pos, uint32_t *last_update_ms, const char *src, 
                            uint8_t window_len, uint8_t gap_len, 
                            uint32_t scroll_speed_ms, uint32_t pause_ms) {
    size_t src_len = strlen(src);

    // If text fits in window, keep position at 0
    if (src_len <= window_len) {
        *scroll_pos = 0;
        return;
    }

    uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
    
    // Pause longer if at start (scroll_pos == 0), otherwise use normal scroll speed
    uint32_t required_delay = (*scroll_pos == 0) ? pause_ms : scroll_speed_ms;

    if (current_time_ms - *last_update_ms >= required_delay) {
        *scroll_pos = (*scroll_pos + 1) % (src_len + gap_len);
        *last_update_ms = current_time_ms;
    }
}

// Helper function to build a wrapped marquee string into a destination buffer
static void render_marquee_text(char *dest, const char *src, uint16_t scroll_pos, uint8_t window_len, uint8_t gap_len) {
    uint8_t src_len = strlen(src);

    // Case 1: Text fits inside the window without scrolling
    if (src_len <= window_len) {
        strncpy(dest, src, window_len);
        // Pad remaining width with spaces if necessary
        for (size_t i = src_len; i < window_len; i++) {
            dest[i] = ' ';
        }
        dest[window_len] = '\0';
        return;
    }

    // Case 2: Text scrolls with wrap-around gap
    uint8_t total_len = src_len + gap_len;
    uint8_t offset = scroll_pos % total_len;

    for (uint8_t i = 0; i < window_len; i++) {
        size_t idx = (offset + i) % total_len;
        if (idx < src_len) {
            dest[i] = src[idx];  // Title/Artist/Album character
        } else {
            dest[i] = ' ';       // Gap spaces
        }
    }
    dest[window_len] = '\0';
}

static const char* get_artist_name(size_t idx) { return global_artists[idx].artist_name; }
static const char* get_album_name(size_t idx)  { return global_albums[idx].album_name;  }

static void render_infinite_list_marquee(char *dest, uint8_t window_len, 
                                        const char* (*get_name_fn)(size_t idx), 
                                        size_t total_items, 
                                        uint16_t *item_idx, 
                                        uint16_t *char_offset, 
                                        bool advance_step,
                                        bool reverse) {
    if (total_items == 0) {
        snprintf(dest, window_len + 1, "No Items            ");
        return;
    }

    if (advance_step) {
        char current_str[64];
        snprintf(current_str, sizeof(current_str), "%zu. %s   ", *item_idx + 1, get_name_fn(*item_idx));
        size_t len = strlen(current_str);

        if (!reverse) {
            // Forward scrolling (left to right)
            (*char_offset)++;
            if (*char_offset >= len) {
                *char_offset = 0;
                *item_idx = (*item_idx + 1) % total_items;
            }
        } else {
            // Reverse scrolling (right to left)
            if (*char_offset == 0) {
                // Move to previous item in list
                *item_idx = (*item_idx + total_items - 1) % total_items;
                snprintf(current_str, sizeof(current_str), "%zu. %s   ", *item_idx + 1, get_name_fn(*item_idx));
                *char_offset = strlen(current_str) - 1;
            } else {
                (*char_offset)--;
            }
        }
    }

    // Window rendering logic remains unchanged
    int chars_filled = 0;
    size_t temp_idx = *item_idx;
    size_t temp_char = *char_offset;

    while (chars_filled < window_len) {
        char temp_str[64];
        snprintf(temp_str, sizeof(temp_str), "%zu. %s   ", temp_idx + 1, get_name_fn(temp_idx));
        size_t len = strlen(temp_str);

        while (temp_char < len && chars_filled < window_len) {
            dest[chars_filled++] = temp_str[temp_char++];
        }

        if (temp_char >= len) {
            temp_char = 0;
            temp_idx = (temp_idx + 1) % total_items;
        }
    }
    dest[window_len] = '\0';
}

void scrolling_menu(int mode) {
    clear_framebuffer();

    char buf[64];
    char marquee_title[19];  // 18 chars + null
    char marquee_artist[21]; // 20 chars + null
    char marquee_album[21];  // 20 chars + null

    char menu_string[256];
    char info_string_1[256];
    char info_string_2[256];

    uint16_t start;
    uint16_t selected_slot;
    uint16_t item_choice;
    uint16_t item_count;

    if (mode == 1) { // ALBUMS
        item_choice = album_choice;
        item_count = album_count;

        start = (item_choice < 6) ? 0 : item_choice - 5;
        if (item_count >= 10 && start > item_count - 10) {
            start = item_count - 10;
        }

        selected_slot = item_choice - start;
        album_info_t *selected_album = &album_window[selected_slot];

        snprintf(menu_string, sizeof(menu_string), "%s", selected_album->album_name);
        snprintf(info_string_1, sizeof(info_string_1), "%s", current_track->artist);
        if (selected_album->num_tracks == 1) {
            snprintf(info_string_2, sizeof(info_string_2), "1 Track");
        } else {
            snprintf(info_string_2, sizeof(info_string_2), "%u Tracks", selected_album->num_tracks);
        }

    } else if (mode == 2) { // ARTISTS
        item_choice = artist_choice;
        item_count = artist_count;

        start = (item_choice < 6) ? 0 : item_choice - 5;
        if (item_count >= 10 && start > item_count - 10) {
            start = item_count - 10;
        }

        selected_slot = item_choice - start;
        artist_info_t *selected_artist = &artist_window[selected_slot];

        snprintf(menu_string, sizeof(menu_string), "%s", selected_artist->artist_name);
        info_string_1[0] = '\0';
        if (selected_artist->num_albums == 1) {
            snprintf(info_string_2, sizeof(info_string_2), "1 Album");
        } else {
            snprintf(info_string_2, sizeof(info_string_2), "%u Albums", selected_artist->num_albums);
        }
    } else if (mode == 3) { // TRACKS
        item_choice = song_choice;
        item_count = track_count;

        start = (item_choice < 6) ? 0 : item_choice - 5;
        if (item_count >= 10 && start > item_count - 10) {
            start = item_count - 10;
        }

        selected_slot = item_choice - start;
        track_info_t *selected_track = &track_window[selected_slot];

        snprintf(menu_string, sizeof(menu_string), "%s", selected_track->title);
        snprintf(info_string_1, sizeof(info_string_1), "%s", selected_track->artist);
        snprintf(info_string_2, sizeof(info_string_2), "%s", selected_track->album);
    } else { // HOME SCREEN
        item_choice = menu_choice;
        item_count = 8;
        start = 0;

        selected_slot = item_choice - start;

        switch (item_choice) {
            case 1: snprintf(menu_string, sizeof(menu_string), "%d %s", artist_count, (artist_count == 1) ? "Artist" : "Artists"); break;
            case 2: snprintf(menu_string, sizeof(menu_string), "%d %s", album_count, (album_count == 1) ? "Album" : "Albums"); break;
            case 3: snprintf(menu_string, sizeof(menu_string), "%d %s", track_count, (track_count == 1) ? "Track" : "Tracks"); break;
            case 4: snprintf(menu_string, sizeof(menu_string), "Last Played"); break;
            case 5: snprintf(menu_string, sizeof(menu_string), "Shuffle All"); break;
            case 6: snprintf(menu_string, sizeof(menu_string), "Extras"); break;
            case 7: snprintf(menu_string, sizeof(menu_string), "Settings"); break;
            default: menu_string[0] = '\0'; break;
        }

        info_string_1[0] = '\0';
        info_string_2[0] = '\0';
    }

    uint16_t marquee_delay = 1000;
    static uint32_t marquee_delay_start_ms = 0;
    static uint32_t last_marquee_update_ms = 0;

    static uint16_t artist_marquee_idx = 0, artist_char_offset = 0;
    static uint16_t album_marquee_idx  = 0, album_char_offset  = 0;

    uint32_t current_time_ms = to_ms_since_boot(get_absolute_time());
    bool tick = false;

    if (current_time_ms - last_marquee_update_ms >= marquee_scroll_rate) {
        if (current_time_ms - marquee_delay_start_ms >= marquee_delay) {
            tick = true;
            
            // Scroll selected menu title
            if (strlen(menu_string) > 18) {
                marquee_title_start++;
                if (marquee_title_start >= strlen(menu_string) + 6) marquee_title_start = 0;
            }

            // Scroll top metadata line
            if (mode == 0 && item_choice == 4) {
                // Home Screen: "Last Played" track title scrolling
                if (strlen(current_track->title) > 20) {
                    marquee_artist_start++;
                    if (marquee_artist_start >= strlen(current_track->title) + 8) marquee_artist_start = 0;
                }
            } else if (mode != 0 && strlen(info_string_1) > 20) {
                // Submenu artist line scrolling
                marquee_artist_start++;
                if (marquee_artist_start >= strlen(info_string_1) + 8) marquee_artist_start = 0;
            }

            // Scroll bottom metadata line in submenus
            if (mode != 0 && strlen(info_string_2) > 20) {
                marquee_album_start++;
                if (marquee_album_start >= strlen(info_string_2) + 8) marquee_album_start = 0;
            }
        }
        last_marquee_update_ms = current_time_ms;
    }

    render_marquee_text(marquee_title, menu_string, marquee_title_start, 18, 6);

    if (mode == 0) {
        if (item_choice == 4) { // "Last Played" selected
            // Safely format song title (top line) with marquee scrolling/truncation
            render_marquee_text(marquee_artist, current_track->title, marquee_artist_start, 20, 8);

            // Safely format progress time (bottom line)
            snprintf(marquee_album, sizeof(marquee_album), "%d:%02d", progress_min, progress_sec);
        } else {
            // Blank top line on home screen for other options
            memset(marquee_artist, ' ', 20);
            marquee_artist[20] = '\0';

            // Context-sensitive marquee on the bottom row
            if (item_choice == 1) { 
                // "Artists" selected -> Scroll Artist marquee
                render_infinite_list_marquee(marquee_album, 20, get_artist_name, artist_count, 
                                            &artist_marquee_idx, &artist_char_offset, tick, home_marquee_dir);
            } else if (item_choice == 2 || item_choice == 3) { 
                // "Albums" or "Tracks" selected -> Scroll Album marquee
                render_infinite_list_marquee(marquee_album, 20, get_album_name, album_count, 
                                            &album_marquee_idx, &album_char_offset, tick, home_marquee_dir);
            } else {
                // Other Home items -> Clear bottom line
                memset(marquee_album, ' ', 20);
                marquee_album[20] = '\0';
            }
        }
    } else {
        // Standard Submenus
        render_marquee_text(marquee_artist, info_string_1, marquee_artist_start, 20, 8);
        render_marquee_text(marquee_album,  info_string_2, marquee_album_start,  20, 8);
    }

    // Generate List
    for (int i = 0; i < 10; i++) {
        if (start + i >= item_count) break;

        const char *display_name;
        char temp_home_string[32];

        if (mode == 1) {
            display_name = album_window[i].album_name;
        } else if (mode == 2) {
            display_name = artist_window[i].artist_name;
        } else if (mode == 3) {
            display_name = track_window[i].title;
        } else if (mode == 0) {
            switch (start + i) {
                case 1: snprintf(temp_home_string, sizeof(temp_home_string), "%d Artists", artist_count); break;
                case 2: snprintf(temp_home_string, sizeof(temp_home_string), "%d Albums", album_count); break;
                case 3: snprintf(temp_home_string, sizeof(temp_home_string), "%d Tracks", track_count); break;
                case 4: snprintf(temp_home_string, sizeof(temp_home_string), "Last Played"); break;
                case 5: snprintf(temp_home_string, sizeof(temp_home_string), "Shuffle All"); break;
                case 6: snprintf(temp_home_string, sizeof(temp_home_string), "Extras"); break;
                case 7: snprintf(temp_home_string, sizeof(temp_home_string), "Settings"); break;
                default: temp_home_string[0] = '\0'; break;
            }
            display_name = temp_home_string;
            snprintf(buf, sizeof(buf), "%s", (start + i == item_choice) ? marquee_title : display_name);
        }
        
        if (mode != 0) {
            snprintf(buf, sizeof(buf), "%d %s", start + i + 1, (start + i == item_choice) ? marquee_title : display_name);
        }

        uint16_t color = (start + i == item_choice) ? HIGHLIGHT_COLOR_SECONDARY : WHITE;
        st7789_draw_string(1, 0 + i * font_height, buf, color);
    }

    if (mode == 0) {
        st7789_draw_string(1, 0, "Eric's Rock Playlist", HIGHLIGHT_COLOR_PRIMARY);
    }

    // Draw bottom metadata bar
    st7789_draw_string(1, -2 + 10 * font_height, marquee_artist, HIGHLIGHT_COLOR_PRIMARY);
    st7789_draw_string(1, -2 + 11 * font_height, marquee_album,  HIGHLIGHT_COLOR_PRIMARY);

    st7789_set_cursor(0, 0);
    st7789_ramwr();
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write16_blocking(spi0, frame_buffer, 240 * 240);
}

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

uint16_t start;
void core1_entry()
{
    multicore_lockout_victim_init();

    struct repeating_timer anim_timer;
    // add_repeating_timer_ms(500, repeating_timer_callback, NULL, &anim_timer);

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
            // st7789_draw_string(1, 200, current_track->title, WHITE);
            break;
        case 4: // Artists
            process_audio_batch();
            scrolling_menu(2);
            break;
        case 5: // Albums
            process_audio_batch();
            scrolling_menu(1);
            break;
        case 6: // Tracks
            process_audio_batch();
            scrolling_menu(3);
            break;
        case 7:
            scrolling_menu(0);
            break;
        default:
            visualizer = (visualizer == 2 || 3 || 4) ? 6 : 0;
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
        // sleep_us(10);
    }

    // Re-add the bias so the VU meter math processes the peak correctly
    pca9685_update_vu(&vu_meter, ADC_BIAS_CENTER + max_dev_l, ADC_BIAS_CENTER + max_dev_r);
}

#define CENTER_Y 20
#define MARGIN_LEFT 200
#define BAR_WIDTH 4
#define GAP_PX 2

//Adds icons and samples ADC
// Adds icons and samples ADC
void addIcons(uint16_t* frame_buffer, bool enabled) {
    if (enabled) {
        // Place pause Icon on screen
        for (int y = 0; y < 20; y++)
        {
            uint16_t *dst = &frame_buffer[y * SCREEN_WIDTH];
            uint16_t *src = &playStatus[y * 20];
            memcpy(dst, src, 20 * sizeof(uint16_t));
        }
        // Place rewind/fastforward Icon on screen (Starts at x = 24)
        int icon_spacing = 34; // 20px icon width + 4px gap
        for (int y = 0; y < 20; y++)
        {
            uint16_t *dst = &frame_buffer[(y * SCREEN_WIDTH) + icon_spacing];
            uint16_t *src = &ff_rew_status[y * 20];
            memcpy(dst, src, 20 * sizeof(uint16_t));
        }
        // Progress bar
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

        // Track info
        char progress_time[6];
        snprintf(progress_time, sizeof(progress_time), "%d:%02d", progress_min, progress_sec);
        st7789_draw_string(0, 10 + 10 * font_height, progress_time, WHITE);

        // --- SCROLLING TITLE IMPLEMENTATION ---
        static uint8_t scope_pos = 0;
        static uint32_t scope_last_update = 0;

        // Advance position (150ms scroll speed, 2000ms start pause)
        marquee_plus(&scope_pos, &scope_last_update, current_track->title, 18, 6, 150, 2000);

        // Render string
        char title_buf[21];
        render_marquee_text(title_buf, current_track->title, scope_pos, 18, 6);
        st7789_draw_string(30, 1, title_buf, WHITE);
    }
}