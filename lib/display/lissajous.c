#include "lissajous.h"
#include "display.h"        
#include "hardware/spi.h"   
#include "pico/stdlib.h"
#include "lib/sb_util/global_vars.h"
#include "lib/sb_util/sb_util.h"

////////////////LISSAJOUS///////////////////////////

uint16_t dim_pixel(uint16_t color, uint16_t divide)
{
    return color / divide;
}

void draw_line_hot(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1)
    {
        if (x0 >= 0 && x0 < 240 && y0 >= 0 && y0 < 240)
        {
            frame_buffer[y0 * 240 + x0] = color;
        }
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

extern bool enableIcons;

void draw_lissajous()
{
    // 1. Instead of clearing to black, "fade" the previous frame
    // This creates the phosphor trail effect
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++)
    {
        if (frame_buffer[i] != 0)
        {
            frame_buffer[i] = dim_pixel(frame_buffer[i], 2);
        }
    }

    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        float val_l = crealf(audio_history_l[i]);
        float val_r = crealf(audio_history_r[i]);

        // 2. Map and Scale (Same as before)
        int x = (int)(((val_l - ADC_BIAS_CENTER) * 110.0f) / (ADC_RANGE_PKPK / 2.0f)) + 120;
        int y = (int)(((val_r - ADC_BIAS_CENTER) * 110.0f) / (ADC_RANGE_PKPK / 2.0f)) + 120;

        // 3. Clamping
        if (x < 0)
            x = 0;
        if (x > 239)
            x = 239;
        if (y < 0)
            y = 0;
        if (y > 239)
            y = 239;

        // 4. Draw the new sample with FULL brightness
        frame_buffer[y * SCREEN_WIDTH + x] = 0xFFFF;
        frame_buffer[y * SCREEN_WIDTH + (239 - x)] = 0xFFFF;
        frame_buffer[(239 - y) * SCREEN_WIDTH + x] = 0xFFFF;
        frame_buffer[(239 - y) * SCREEN_WIDTH + (239 - x)] = 0xFFFF;
    }
    // 5. Push to Display
    addIcons(frame_buffer, enableIcons);
    st7789_set_cursor(0, 0);
    st7789_ramwr();
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write16_blocking(spi0, frame_buffer, 240 * 240);
}

void draw_lissajous_connected()
{
    for (int i = 0; i < (SCREEN_WIDTH * SCREEN_HEIGHT); i++)
    {
        if (frame_buffer[i] != 0)
        {
            frame_buffer[i] = dim_pixel(frame_buffer[i], 2);
        }
    }

    int last_x = -1, last_y = -1;

    // 2. Connect the dots in the audio history
    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        float val_l = crealf(audio_history_l[i]);
        float val_r = crealf(audio_history_r[i]);

        // Map to screen coordinates
        int x = (int)(((val_l - ADC_BIAS_CENTER) * 110.0f) / (ADC_RANGE_PKPK / 2.0f)) + 120;
        int y = (int)(((val_r - ADC_BIAS_CENTER) * 110.0f) / (ADC_RANGE_PKPK / 2.0f)) + 120;

        // If we have a previous point, draw a line to the current one
        if (last_x != -1)
        {
            draw_line_hot(last_x, last_y, x, y, 0xFFFF); // Hot White Line
        }

        last_x = x;
        last_y = y;
    }
    addIcons(frame_buffer, enableIcons);
    st7789_set_cursor(0, 0);
    st7789_ramwr();
    spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    spi_write16_blocking(spi0, frame_buffer, 240 * 240);
}

////////////////////LISSAJOUS////////////////////////////