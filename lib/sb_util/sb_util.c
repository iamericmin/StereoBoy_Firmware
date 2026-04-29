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

////////////////////IMAGE////////////////////////////

typedef struct
{
    FIL *fil;
    uint32_t bytes_left;
} jpeg_stream_t;

unsigned char jpeg_need_bytes_callback(
    unsigned char *pBuf,
    unsigned char buf_size,
    unsigned char *pBytes_actually_read,
    void *pCallback_data)
{
    jpeg_stream_t *ctx = (jpeg_stream_t *)pCallback_data;

    if (ctx->bytes_left == 0)
    {
        *pBytes_actually_read = 0;
        return 0; // EOF is OK for picojpeg
    }

    UINT to_read = buf_size;
    if (to_read > ctx->bytes_left)
        to_read = ctx->bytes_left;

    UINT br;
    if (f_read(ctx->fil, pBuf, to_read, &br) != FR_OK)
        return PJPG_STREAM_READ_ERROR;

    *pBytes_actually_read = br;
    ctx->bytes_left -= br;

    return 0;
}

void process_image(track_info_t *track, const char *filename, float output_size)
{
    FIL fil;
    UINT br;
    uint8_t header[10];
    uint8_t frame_header[10];

    if (f_open(&fil, filename, FA_READ) != FR_OK)
    {
        return;
    }

    if (f_read(&fil, header, 10, &br) != FR_OK || br != 10)
    {
        goto out;
    }

    if (memcmp(header, "ID3", 3) != 0)
    {
        goto out;
    }

    f_lseek(&fil, track->album_art_offset);
    if (strcmp(track->mime_type, "image/jpeg") == 0)
    {
        pjpeg_image_info_t jpeg_info;

        jpeg_stream_t stream = {
            .fil = &fil,
            .bytes_left = track->album_art_size};

        unsigned char status =
            pjpeg_decode_init(&jpeg_info, jpeg_need_bytes_callback, &stream, 0);

        if (status)
        {
            memset(img_buffer, 0, sizeof(img_buffer));
            goto out;
        }

        float scale_x = (float)jpeg_info.m_width / output_size;
        float scale_y = (float)jpeg_info.m_height / output_size;

        for (uint16_t my = 0; my < jpeg_info.m_MCUSPerCol; my++)
        {
            for (uint16_t mx = 0; mx < jpeg_info.m_MCUSPerRow; mx++)
            {
                status = pjpeg_decode_mcu();

                if (status == PJPG_NO_MORE_BLOCKS)
                {
                    break;
                }
                if (status)
                {
                    goto out;
                }
                for (uint16_t ly = 0; ly < jpeg_info.m_MCUHeight; ly++)
                {
                    for (uint16_t lx = 0; lx < jpeg_info.m_MCUWidth; lx++)
                    {
                        uint16_t src_x = mx * jpeg_info.m_MCUWidth + lx;
                        uint16_t src_y = my * jpeg_info.m_MCUHeight + ly;

                        uint16_t dst_x = src_x / scale_x;
                        uint16_t dst_y = src_y / scale_y;

                        if (dst_x >= output_size || dst_y >= output_size)
                            continue;

                        uint16_t idx = ly * jpeg_info.m_MCUWidth + lx;

                        uint8_t r = jpeg_info.m_pMCUBufR[idx];
                        uint8_t g = jpeg_info.m_pMCUBufG[idx];
                        uint8_t b = jpeg_info.m_pMCUBufB[idx];

                        uint16_t rgb565 =
                            ((r & 0xF8) << 8) |
                            ((g & 0xFC) << 3) |
                            (b >> 3);

                        img_buffer[dst_y * IMG_WIDTH + dst_x] = rgb565;
                    }
                }
            }
            if (status == PJPG_NO_MORE_BLOCKS)
            {
                break;
            }
        }
    }
out:
    f_close(&fil);
}

////////////////////IMAGE////////////////////////////



int get_selected_band(){
    return selected_band;
}

// Headphones disconnect interrupt
void dac_int_callback(uint gpio, uint32_t events)
{
    // Read 0x2C to clear the sticky interrupt
    dac_read(0, 0x2C); // THIS NEEDS TO BE HERE!!!! DO NOT REMOVE THIS LINE
    // read whether headphone in or out
    playStatus = pause_icon;
    if (dac_read(0, 0x2E) & 0x10)
    { // Bit 5
        dac_write(1, 0x20, 0b00000110); // shut down speaker driver
        // pause without warping
        paused = 1;
        warping = 0;
        // Reg 0x1F: HP Drivers power up
        dac_write(1, 0x1F, 0xC0);
        // Reg 0x28/0x29: HPL/R Driver unmute
        dac_write(1, 0x28, 0x06);
        dac_write(1, 0x29, 0x06);
        printf("Headphones plugged in! Paused and switching to stereo headphones.\n");
    }
    else
    {
        printf("Headphones pulled out! Paused and switching to mono speakers.\n");
        dac_write(1, 0x20, 0b10000110); // power up speaker driver
        // pause without warping
        paused = 1;
        warping = 0;
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
