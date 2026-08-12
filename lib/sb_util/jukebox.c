#include "global_vars.h"
#include "sb_util.h"

/* ##########################################################
JUKEBOX: MAIN PLAY LOOP
########################################################## */

bool paused = false;
bool warping = false;
bool stopped = false;
bool fast_forward = false;
bool audio_rewind = false;
bool enableIcons;
uint16_t normal_speed = 1; // 1 = normal

volatile uint16_t potVal = 0;

#define PAUSE_WARP_US 600000   // 0.7 seconds for pause
#define RESUME_WARP_US 1200000 // 1.2 seconds for resume
#define SKIP_INTERVAL_MS 100   // minimum interval between FF/RW jumps

int selected_band = 0;
uint16_t *playStatus = empty_icon;
uint16_t *ff_rew_status = empty_icon;

int progress_bar = 0;
int prev_progress_bar = 0;

/*******************visualizations not scope*******************/
#define HISTORY_SIZE 256
cplx audio_history_l[HISTORY_SIZE];
cplx audio_history_r[HISTORY_SIZE];
int history_index = 0;
int num_visualizations = 8;
bool album_art_ready = false;

int jukebox()
{
    FIL fil;             // file object
    UINT br;             // pointer to number of bytes read
    uint8_t buffer[2048]; // buffer read from file

    char *filename = current_track->filename;
    uint16_t sampleSpeed = current_track->samplespeed;
    uint16_t bitRate = current_track->bitrate;
    uint32_t skip_bits = bitRate * 256; // bitrate * 1024 / 4 = approx. 2 seconds
    int exitType = 0;
    sci_write(&player, 0x05, sampleSpeed + 1); // initialize codec sampling speed (+1 at the end for stereo)

    // status bits for &player state and warp effect
    paused = false;
    playStatus = play_icon;
    warping = false;
    stopped = 0;
    enableIcons = true;

    // more warp effect stuff
    float transport = 1.0f;                  // desired speed
    float warp_start_transport = 1.0f;       // start speed for warp
    float warp_target = 1.0f;                // target speed for warp
    uint32_t warp_duration = RESUME_WARP_US; // warp effect duration
    absolute_time_t warp_start_time;

    // open selected MP3 file
    if (f_open(&fil, filename, FA_READ) != FR_OK)
    {
        printf("Failed to open %s\r\n", filename);
        return exitType;
    }

    uint16_t stereo_bit = sampleSpeed & 1;     // LSB indicates mono or stereo (not exactly sure what but this is pretty much always 1)
    uint16_t base_rate = sampleSpeed & 0xFFFE; // sampling speed in upper 15 bits
    if (visualizer == 0) {
        display_album_art_by_index(img_buffer, song_choice);
    }

    f_lseek(&fil, current_track->audio_start);
    absolute_time_t last_skip_time = get_absolute_time();

    selected_band = 0;
    int currEq = 0;
    dac_eq_init(sampleSpeed); // init with default sample rated
    uint8_t vol_check = 5;
    uint8_t old_volume = 0;
    // read_lwbt();

    while (1)
    {

        // Always feed decoder unless fully paused
        if (!paused || warping)
        {
            uint16_t new_rate = (uint16_t)(base_rate * transport) & 0xFFFE;
            if (new_rate < 9000)
                new_rate = 9000;
            sci_write(&player, 0x05, new_rate | stereo_bit);

            if (f_read(&fil, buffer, sizeof(buffer), &br) != FR_OK || br == 0)
            {
                exitType = 1; // Default return when no bytes read (end of song)
                break;
            }

            vs1053_play_data(&player, buffer, br);
        }
        
        // janky counter for volume sampling
        if (vol_check == 5) {
            uint16_t vol = (uint32_t)potVal * 0x60 / 4096;
            ff_rew_status = empty_icon; //update ff/rew icon every 10 as well
            dac_set_volume(vol);
            vol_check = 0;
        } else {
            vol_check++;
        }
        
        int c = getchar_timeout_us(0); // nonblocking getchar

        // get value from buttons
        if (c == PICO_ERROR_TIMEOUT)
        {
            char btn_char = get_button_jukebox(selected_band);
            if (btn_char != 0)
                c = (int)btn_char; // Inject the button character into the logic
        }

        //progress bar (should make separate function)
        long song_pos = f_tell(&fil);
        float progress = (float)(song_pos - current_track->audio_start) / (float)(current_track->audio_end - current_track->audio_start);
        if (progress < 0.0f)
            progress = 0.0f;
        if (progress > 1.0f)
            progress = 1.0f;
        prev_progress_bar = progress_bar;
        progress_bar = 240 * progress;
        bool update_bar = prev_progress_bar != progress_bar;

        if (c != PICO_ERROR_TIMEOUT)
        {
            long pos = f_tell(&fil);
            // bool headphonesIn = dac_read(0, 0x43) & 0x20;
            // printf("Headphone prescence: %d\r\n", headphonesIn);
            absolute_time_t now = get_absolute_time();

            // EQ START
            //  Select the band (keys 0-5)
            if (c >= '0' && c <= '5')
            {
                selected_band = c - '0';
                printf("\nSelected Band: %d Hz\n", dac_eq_get_freq(selected_band));
            }

            // Adjust the band (+ or -)
            if (c == '+' || c == '=')
            {
                dac_write(0, 0x3F, 0b11111110); // set audio output to mono
                // transport += 0.05;
                // dac_eq_adjust(selected_band, 0.5f, sampleSpeed); // Boost
                // printf("Band %d Gain: %.1f dB\n", selected_band, dac_eq_get_gain(selected_band));
            }
            if (c == '-')
            {
                dac_write(0, 0x3F, 0b11010110); // set audio output to stereo
                // transport -= 0.05;
                // dac_eq_adjust(selected_band, -0.5f, sampleSpeed); // Cut
                // printf("Band %d Gain: %.1f dB\n", selected_band, dac_eq_get_gain(selected_band));
            }
            // EQ END

            switch (c)
            {
            // new **
            case 'n':
            case 'N':
                if (visualizer == 6) {
                    // multicore_lockout_start_blocking();
                    if (song_choice + 10 > track_count) {
                        song_choice = track_count % 10;
                    } else {
                        song_choice += 10;
                    }
                    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
                        printf("Error reading track metadata from cache table!\n");
                    }
                    printf("\nUp by 10! Track: %d\r\n", song_choice);
                    // multicore_lockout_end_blocking();
                    break;
                } else {
                    exitType = 1;
                    vs1053_set_play_speed(&player, 0); // hard pause
                    printf("\r\n Going to next song....\r\n");
                    f_close(&fil);
                    vs1053_stop(&player);
                    return exitType;
                }
            case 'o':
            case 'O':
                if (visualizer == 6) { // scroll through menu without actually changing the track
                    // multicore_lockout_start_blocking();
                    if (song_choice - 10 < 1) {
                        song_choice = track_count - (track_count % 10);
                    } else {
                        song_choice -= 10;
                    }
                    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
                        printf("Error reading track metadata from cache table!\n");
                    }
                    printf("\rDown by 10! Track: %d\r\n", song_choice);
                    // multicore_lockout_end_blocking();
                    break;
                } else {
                    uint8_t seconds_into_song = (f_tell(&fil) - current_track->audio_start) / (current_track->bitrate * 125);
                    if (seconds_into_song >= 5){
                        // uint32_t audio_start = find_audio_start(&fil);
                        f_lseek(&fil, current_track->audio_start);
                        break;
                    } else {
                        exitType = 2;
                        vs1053_set_play_speed(&player, 0); // hard pause
                        printf("\r\n Going to previous song....\r\n");
                        f_close(&fil);
                        vs1053_stop(&player);
                        return exitType;
                    }
                }
            case 'p':
            case 'P':
                if (visualizer == 6) {
                    paused = 0;
                    warping = 0;
                    playStatus = play_icon;
                    exitType = 3;
                    f_close(&fil);
                    vs1053_stop(&player);
                    return exitType;
                } else {
                    paused = !paused;                      // set paused flag
                    warp_start_time = get_absolute_time(); // get timestamp for warp start
                    warp_start_transport = transport;      //
                    warp_target = paused ? 0.0f : 1.0f;
                    warping = true;
                    if (paused) playStatus = pause_icon;
                    else playStatus = play_icon;

                    // select duration based on pause/resume
                    warp_duration = paused ? PAUSE_WARP_US : RESUME_WARP_US;

                    printf(paused ? "\r\nTape slowing...\r\n"
                                : "\r\nTape resuming...\r\n");
                    break;
                }
            case 'f':
            case 'F':
                ff_rew_status = ff_icon;
                pos += skip_bits;
                if (pos > f_size(&fil))
                    pos = f_size(&fil) - 1;
                f_lseek(&fil, pos);
                printf("\r\nFast-forwarded ~2s\r\n");
                last_skip_time = now;
                break;
            case 'r':
            case 'R':
                ff_rew_status = rew_icon;
                pos -= skip_bits;
                if (pos < 0)
                    pos = 0;
                f_lseek(&fil, pos);
                printf("\r\nRewound ~2s\r\n");
                last_skip_time = now;
                break;
            case 'u':
            case 'U':
                if (visualizer == 6) { // scroll through menu without actually changing the track
                    if (song_choice - 1 < 1) {
                        song_choice = track_count - 1;
                    } else {
                        song_choice -= 1;
                    }
                    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
                        printf("Error reading track metadata from cache table!\n");
                    }
                    printf("\r\nUp by 1! Track: %d\r\n", song_choice);
                } else {
                    uint8_t seconds_into_song = (f_tell(&fil) - current_track->audio_start) / (current_track->bitrate * 125);
                    if (seconds_into_song >= 5){
                        // uint32_t audio_start = find_audio_start(&fil);
                        f_lseek(&fil, current_track->audio_start);
                        break;
                    } else {
                        exitType = 2;
                        vs1053_set_play_speed(&player, 0); // hard pause
                        printf("\r\n Going to next song....\r\n");
                        f_close(&fil);
                        vs1053_stop(&player);
                        return exitType;
                    }
                }
                break;
            case 'd':
            case 'D':
                if (visualizer == 6) {
                    if (song_choice + 1 > track_count) {
                        song_choice = 0;
                    } else {
                        song_choice += 1;
                    }
                    if (!sb_get_track_window_fast(&tracks_cache_file, song_choice, current_track, track_window)) {
                        printf("Error reading track metadata from cache table!\n");
                    }
                    printf("\rDown by 1! Track: %d\r\n", song_choice);
                } else {
                    exitType = 1;
                    vs1053_set_play_speed(&player, 0); // hard pause
                    printf("\r\n Going to next song....\r\n");
                    f_close(&fil);
                    vs1053_stop(&player);
                    return exitType;
                }
                break;
            case 'l':
            case 'L':
                pca9685_toggleSleep(&vu_meter);
                break;
            case 'v':
            case 'V':
                visualizer = (visualizer + 1) % (num_visualizations - 1);
                if (visualizer == 0) {
                    display_album_art_by_index(img_buffer, song_choice);
                    printf("changing visualizer");
                }
                switch (visualizer) {
                case 0:
                    printf("\r\nAlbum Art Visualization\r\n");
                    break;
                case 1:
                    printf("\r\nScope Visualization\r\n");
                    break;
                case 2:
                    printf("\r\nSpectrum Analyzer Visualization\r\n");
                    break;
                case 3:
                    printf("\r\nLissajous Visualization\r\n");
                    break;
                case 4:
                    printf("\r\nMandala Visualization\r\n");
                    break;
                case 5:
                    dprint("Text Display");
                    printf("\r\nText Display\r\n");
                    break;
                case 6:
                    dprint("Main Menu");
                }
                break;
            case 'i':
            case 'I':
                printf("\r\n\rNOW PLAYING:\r\n");
                printf("  Title : %s\r\n", current_track->title);
                printf("  Artist: %s\r\n", current_track->artist);
                printf("  Album : %s\r\n", current_track->album);
                printf("  Bitrate : %d Kbps\r\n", current_track->bitrate);
                printf("  Sample rate : %d Hz\r\n", current_track->samplespeed);
                printf("  Channels : %s\r\n", current_track->channels == 1 ? "Mono" : "Stereo");
                printf("  Header: %X\r\n", current_track->header);
                break;
            case 'm':
            case 'M':
                enableIcons = !enableIcons;
                break;
            case 's':
            case 'S':
                if (paused)
                {
                    exitType = 0;
                    vs1053_set_play_speed(&player, 0); // hard pause
                    printf("\r\nStopping....\r\n");
                    f_close(&fil);
                    vs1053_stop(&player);
                    return exitType;
                }
                stopped = 1;
                warp_start_time = get_absolute_time();
                warp_start_transport = transport;
                warp_target = 0.0f;
                warp_duration = PAUSE_WARP_US;
                warping = true;
                // album_art_ready = false;
                break;
            }
        }

        // --- Warp logic ---
        if (warping)
        {
            int64_t elapsed = absolute_time_diff_us(warp_start_time, get_absolute_time());

            if (elapsed >= warp_duration)
            {
                transport = warp_target;
                warping = false;

                if (paused)
                {
                    vs1053_set_play_speed(&player, 0); // hard pause
                    printf("\r\nPaused.\r\n");
                }
                else if (stopped)
                {
                    vs1053_set_play_speed(&player, 0); // hard pause
                    printf("\r\nPaused.\r\n");
                    f_close(&fil);
                    vs1053_stop(&player);
                    return 0;
                }
            }
            else
            {
                float t = (float)elapsed / (float)warp_duration;
                transport = warp_start_transport +
                            (warp_target - warp_start_transport) * t;
            }
        }
    }

    f_close(&fil);
    // exitType = 0; //plays next song if song just ends
    return exitType;
}
