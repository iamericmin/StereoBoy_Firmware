#include "sb_util.h"

uint16_t frame_buffer[240 * 240];
uint16_t img_buffer[IMG_WIDTH * IMG_HEIGHT];
pca9685_t vu_meter;

/*******************visualizations not scope*******************/

/* =========================================================
   PRIVATE HELPERS (static)
   ========================================================= */

/* =========================================================
   PUBLIC API
   ========================================================= */
void clear_framebuffer()
{
    mutex_enter_blocking(&text_buff_mtx);
    memset(frame_buffer, 0, sizeof(frame_buffer));
    mutex_exit(&text_buff_mtx);
}


void sb_print_track(track_info_t *t)
{
    printf("\n%s - %s\n", t->artist, t->title);
    printf("Album: %s\n", t->album);
    printf("%d kbps  %d Hz  %s\n",
           t->bitrate,
           t->samplespeed,
           t->channels ? "Mono" : "Stereo");
}

int get_selected_band(){
    return selected_band;
}

// Headphones disconnect interrupt
void dac_int_callback(uint gpio, uint32_t events)
{
    // Read 0x2C to clear the sticky interrupt
    dac_read(0, 0x2C); // THIS NEEDS TO BE HERE!!!! DO NOT REMOVE THIS LINE
    playStatus = pause_icon;
    if (dac_read(0, 0x2E) & 0x10) { // read whether headphone in or out
        printf("Headphones plugged in! Paused and switching to stereo headphones.\n");
        // pause without warping
        paused = 1;
        warping = 0;
        dac_write(1, 0x20, 0b00000110); // shut down speaker driver
        dac_write(0, 0x3F, 0b11010110); // set audio output to stereo
        dac_eq_init(current_track->samplespeed); // init with default sample rated
        dac_eq_adjust(selected_band, 0.50f, current_track->samplespeed); // Bass Boost
        // Reg 0x1F: HP Drivers power up
        dac_write(1, 0x1F, 0xC0);
        // Reg 0x28/0x29: HPL/R Driver unmute
        dac_write(1, 0x28, 0x06);
        dac_write(1, 0x29, 0x06);
    } else {
        printf("Headphones pulled out! Paused and switching to mono speakers.\n");
        // pause without warping
        paused = 1;
        warping = 0;
        dac_write(1, 0x1F, 0x00); // Reg 0x1F: HP Drivers power down
        dac_write(0, 0x3F, 0b11111110); // set audio output to mono
        dac_eq_init(current_track->samplespeed); // init with default sample rated
        dac_write(1, 0x20, 0b10000110); // power up speaker driver
    }
}

// ---- Init GPIO interrupt ----
void dac_interrupt_init(void)
{
    gpio_init(3);
    gpio_set_dir(3, GPIO_IN);
    gpio_pull_up(3); // INT is usually open-drain

    gpio_set_irq_enabled_with_callback(
        3,
        GPIO_IRQ_EDGE_RISE, // active-low interrupt
        true,
        &dac_int_callback);
}
