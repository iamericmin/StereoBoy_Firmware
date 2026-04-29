#include "lib/sb_util/sb_util.h"
// Convert syncsafe integer (ID3 size format)

uint32_t syncsafe_to_uint(const uint8_t *b)
{
    return (b[0] << 21) | (b[1] << 14) | (b[2] << 7) | b[3];
}

void read_text_frame(FIL *fil, uint32_t frame_size, char *out, size_t out_size)
{
    UINT br;
    uint8_t encoding;

    f_read(fil, &encoding, 1, &br);
    frame_size--;

    memset(out, 0, out_size);

    // ---------------- UTF-8 ----------------
    if (encoding == 3)
    {
        uint32_t n = (frame_size < out_size - 1) ? frame_size : out_size - 1;
        f_read(fil, out, n, &br);
        out[n] = '\0';
        return;
    }

    // ---------------- ISO-8859-1 → UTF-8 ----------------
    if (encoding == 0)
    {
        uint8_t b;
        size_t oi = 0;

        for (uint32_t i = 0; i < frame_size && oi < out_size - 1; i++)
        {
            f_read(fil, &b, 1, &br);
            if (b < 0x80)
            {
                out[oi++] = b;
            }
            else
            {
                if (oi + 2 >= out_size)
                    break;
                out[oi++] = 0xC0 | (b >> 6);
                out[oi++] = 0x80 | (b & 0x3F);
            }
        }
        out[oi] = '\0';
        return;
    }

    // ---------------- UTF-16 → UTF-8 ----------------
    if (encoding == 1 || encoding == 2)
    {
        bool little_endian = true;
        uint8_t bom[2];

        // Read BOM if present
        f_read(fil, bom, 2, &br);
        frame_size -= 2;

        if (encoding == 1)
        {
            if (bom[0] == 0xFE && bom[1] == 0xFF)
                little_endian = false;
            else if (bom[0] == 0xFF && bom[1] == 0xFE)
                little_endian = true;
        }
        else
        {
            // UTF-16BE without BOM
            little_endian = false;
        }

        size_t oi = 0;
        for (uint32_t i = 0; i + 1 < frame_size && oi < out_size - 1; i += 2)
        {
            uint8_t b1, b2;
            f_read(fil, &b1, 1, &br);
            f_read(fil, &b2, 1, &br);

            uint16_t ch = little_endian ? (b1 | (b2 << 8)) : ((b1 << 8) | b2);
            if (ch == 0x0000)
                break;

            if (ch < 0x80)
            {
                out[oi++] = ch;
            }
            else if (ch < 0x800 && oi + 2 < out_size)
            {
                out[oi++] = 0xC0 | (ch >> 6);
                out[oi++] = 0x80 | (ch & 0x3F);
            }
            else if (oi + 3 < out_size)
            {
                out[oi++] = 0xE0 | (ch >> 12);
                out[oi++] = 0x80 | ((ch >> 6) & 0x3F);
                out[oi++] = 0x80 | (ch & 0x3F);
            }
        }

        out[oi] = '\0';
        return;
    }

    // Unknown encoding → skip
    f_lseek(fil, f_tell(fil) + frame_size);
}

uint32_t find_audio_start(FIL *fil)
{
    UINT br;
    uint8_t header[10];

    f_lseek(fil, 0);

    if (f_read(fil, header, 10, &br) != FR_OK || br != 10)
        return 0;

    if (memcmp(header, "ID3", 3) == 0)
    {
        uint32_t tag_size = syncsafe_to_uint(&header[6]);
        return 10 + tag_size;
    }

    // No ID3 tag → audio starts at 0
    return 0;
}

void get_mp3_header(FIL *fil, track_info_t *track)
{
    UINT br;
    uint8_t header[4];

    // Find first frame header (0x7FF)
    while (1)
    {
        if (f_read(fil, &header[0], 1, &br) != FR_OK || br != 1)
            return;

        // First 8 sync bits must be all ones
        if (header[0] != 0xFF)
            continue;

        if (f_read(fil, &header[1], 1, &br) != FR_OK || br != 1)
            return;

        // Next 3 bits must also be ones (111xxxxx)
        if ((header[1] & 0xE0) != 0xE0)
        {
            // Not a real sync → rewind 1 byte so we don't skip potential syncs
            f_lseek(fil, f_tell(fil) - 1);
            continue;
        }

        // We now have a valid 11-bit sync → read remaining 2 bytes
        if (f_read(fil, &header[2], 2, &br) != FR_OK || br != 2)
            return;

        track->audio_start = f_tell(fil) - 4;

        break; // Valid frame header found
    }

    track->header =
        ((uint32_t)header[0] << 24) |
        ((uint32_t)header[1] << 16) |
        ((uint32_t)header[2] << 8) |
        ((uint32_t)header[3]);

    uint8_t version_bits = (header[1] >> 3) & 0x03; // MPEG version
    uint8_t layer_bits = (header[1] >> 1) & 0x03;   // Layer
    uint8_t bitrate_bits = (header[2] >> 4) & 0x0F;
    uint8_t samplespeed_bits = (header[2] >> 2) & 0x03;
    uint8_t channel_bits = (header[3] >> 6) & 0x03;

    // MPEG version
    switch (version_bits)
    {
    case 0:
        track->mpegID = 2;
        break; // MPEG 2.5
    case 2:
        track->mpegID = 2;
        break; // MPEG 2
    case 3:
        track->mpegID = 1;
        break; // MPEG 1
    default:
        track->mpegID = 0;
        break; // reserved/unknown
    }

    // Sample rates table (Hz)
    const uint16_t samplespeeds[4][4] = {
        {11025, 12000, 8000, 0},  // MPEG 2.5  [00]
        {0, 0, 0, 0},             // reserved  [01]
        {22050, 24000, 16000, 0}, // MPEG 2    [10]
        {44100, 48000, 32000, 0}  // MPEG 1    [11]
    };

    track->samplespeed = samplespeeds[version_bits][samplespeed_bits];

    // Bitrate tables
    const uint16_t v1_bitrates[4][16] = {
        // Layer 0 (should never happen)
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        // V1 L3
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
        // V1 L2
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
        // V1 L1
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0}};

    const uint16_t v2_bitrates[4][16] = {
        // Layer 0 (should never happen)
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        // V2, L3
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
        // V2, L2
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
        // V2 L1
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0}};

    if (track->mpegID == 2)
    {
        track->bitrate = v2_bitrates[layer_bits][bitrate_bits]; // if MPEG Version 2
    }
    else if (track->mpegID == 1)
    {
        track->bitrate = v1_bitrates[layer_bits][bitrate_bits]; // if MPEG Version 1
    }
    else
    {
        track->bitrate = 0;
    }

    // Channels
    track->channels = (channel_bits >> 1) & 1; // 0 = stereo, 1 = mono
}

/**
 * Extracts metadata (Title, Artist, Album, Album Art) from an MP3 file.
 * This implementation supports ID3v2 tags at the start and ID3v1 at the end.
 */
void get_mp3_metadata(const char *filename, track_info_t *track)
{
    // Initialize track structure with default values to avoid garbage data
    strcpy(track->filename, filename);
    strcpy(track->title, "(unknown)");
    strcpy(track->artist, "(unknown)");
    strcpy(track->album, "(unknown)");
    strcpy(track->mime_type, "unknown");
    track->album_art_size = 0;
    track->album_art_offset = 0;
    track->album_art_type = 0;

    FIL fil;
    UINT br;
    uint8_t header[10];
    uint8_t frame_header[10];

    // Attempt to open the file using FatFs
    if (f_open(&fil, filename, FA_READ) != FR_OK)
        return;

    // Read the first 10 bytes to check for the ID3v2 header
    if (f_read(&fil, header, 10, &br) != FR_OK || br != 10)
        goto out;

    // Verify 'ID3' identifier; if not found, it's not a standard ID3v2 file
    if (memcmp(header, "ID3", 3) != 0)
        goto out;

    // Convert the 4-byte syncsafe integer to a standard uint32
    // Syncsafe integers ignore the 7th bit of every byte (0xxxxxxx)
    uint32_t tag_size = syncsafe_to_uint(&header[6]);
    uint32_t bytes_read = 0;

    // Iterate through frames until we've parsed the entire ID3 header block
    while (bytes_read < tag_size)
    {
        // Read the 10-byte frame header (ID, Size, Flags)
        if (f_read(&fil, frame_header, 10, &br) != FR_OK || br != 10)
            break;

        bytes_read += 10;
        
        // ID3 padding: if the first byte of a frame ID is 0, we've hit the end of the tags
        if (frame_header[0] == 0)
            break;

        // Extract the 4-character Frame ID (e.g., "TIT2", "APIC")
        char id[5];
        memcpy(id, frame_header, 4);
        id[4] = 0;

        // Calculate frame size (Note: ID3v2.3 uses normal bytes, v2.4 uses syncsafe here)
        uint32_t size =
            (frame_header[4] << 24) |
            (frame_header[5] << 16) |
            (frame_header[6] << 8) |
            frame_header[7];

        // Route specific frames to their respective handlers
        if (!strcmp(id, "TIT2")) // Title
        {
            read_text_frame(&fil, size, track->title, sizeof(track->title));
        }
        else if (!strcmp(id, "TPE1")) // Artist
        {
            read_text_frame(&fil, size, track->artist, sizeof(track->artist));
        }
        else if (!strcmp(id, "TALB")) // Album
        {
            read_text_frame(&fil, size, track->album, sizeof(track->album));
        }
        else if (!strcmp(id, "APIC")) // Attached Picture
        {
            // Note the position exactly after the frame header
            FSIZE_t frame_start_pos = f_tell(&fil);
            UINT br;
            uint8_t encoding;

            // 1. Read Text Encoding (0=ISO-8859-1, 1=UTF-16, etc.)
            f_read(&fil, &encoding, 1, &br);

            // 2. Read MIME Type (e.g., "image/jpeg") - Handled byte-by-byte
            char temp_mime[32];
            int i = 0;
            do
            {
                f_read(&fil, &temp_mime[i], 1, &br);
            } while (temp_mime[i++] != '\0' && i < 31);

            // 3. Read Picture Type (e.g., 0x03 is Front Cover)
            uint8_t pic_type;
            f_read(&fil, &pic_type, 1, &br);

            // Logic: Prioritize Front Cover (0x03), otherwise take the first image found
            if (pic_type == 0x03 || track->album_art_offset == 0)
            {
                track->album_art_type = pic_type;
                strncpy(track->mime_type, temp_mime, sizeof(track->mime_type));

                // 4. Skip the Description string (variable length, null-terminated)
                char dummy;
                if (encoding == 0)
                { 
                    // Single null terminator for standard ASCII/ISO strings
                    do
                    {
                        f_read(&fil, &dummy, 1, &br);
                    } while (dummy != '\0');
                }
                else
                { 
                    // Double null terminator for UTF-16 strings
                    uint16_t dummy16;
                    do
                    {
                        f_read(&fil, &dummy16, 2, &br);
                    } while (dummy16 != 0x0000);
                }

                // 5. Store the file pointer location where the actual image binary starts
                track->album_art_offset = f_tell(&fil);

                // 6. Calculate image size by subtracting metadata overhead from total frame size
                track->album_art_size = size - (track->album_art_offset - frame_start_pos);
            }

            // 7. Seek to the absolute end of the frame to keep the loop aligned
            f_lseek(&fil, frame_start_pos + size);
        }
        else
        {
            // Skip unknown/unsupported frames
            f_lseek(&fil, f_tell(&fil) + size);
        }

        bytes_read += size;
    }

    // Attempt to extract bitrate/duration from the MPEG header
    get_mp3_header(&fil, track);
    
    uint32_t file_size = f_size(&fil);
    track->audio_end = file_size;

    // ID3v1 Check: Look for the 128-byte "TAG" block at the very end of the file
    if (file_size > 128)
    {
        uint8_t tag_buf[3];
        f_lseek(&fil, file_size - 128);
        if (f_read(&fil, tag_buf, 3, &br) == FR_OK && br == 3)
        {
            if (memcmp(tag_buf, "TAG", 3) == 0)
            {
                // If ID3v1 exists, the actual audio data ends 128 bytes before EOF
                track->audio_end = file_size - 128;
            }
        }
    }

out:
    f_close(&fil); // Ensure file is closed even if an error occurs (via goto)
}

// Helper for qsort
int compare_filenames(const void *a, const void *b)
{
    const track_info_t *ta = (const track_info_t *)a;
    const track_info_t *tb = (const track_info_t *)b;
    return strcasecmp(ta->filename, tb->filename);
}
