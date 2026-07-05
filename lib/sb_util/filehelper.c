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
    
    if (frame_size == 0) return;

    // Read the encoding byte
    if (f_read(fil, &encoding, 1, &br) != FR_OK || br == 0) return;
    frame_size--;

    // Use a local buffer to avoid byte-by-byte f_read calls.
    // 256 bytes is usually enough for titles/artists.
    uint8_t buffer[256]; 
    uint32_t to_read = (frame_size > sizeof(buffer)) ? sizeof(buffer) : frame_size;
    
    if (f_read(fil, buffer, to_read, &br) != FR_OK) return;

    memset(out, 0, out_size);
    size_t oi = 0;
    size_t max_oi = out_size - 1;

    // ---------------- UTF-8 (Encoding 3) ----------------
    if (encoding == 3)
    {
        size_t n = (br < max_oi) ? br : max_oi;
        memcpy(out, buffer, n);
        out[n] = '\0';
    }
    // ---------------- ISO-8859-1 → UTF-8 (Encoding 0) ----------------
    else if (encoding == 0)
    {
        for (uint32_t i = 0; i < br && oi < max_oi; i++)
        {
            uint8_t b = buffer[i];
            if (b < 0x80) {
                out[oi++] = b;
            } else if (oi + 1 < max_oi) {
                out[oi++] = 0xC0 | (b >> 6);
                out[oi++] = 0x80 | (b & 0x3F);
            }
        }
        out[oi] = '\0';
    }
    // ---------------- UTF-16 → UTF-8 (Encoding 1 or 2) ----------------
    else if (encoding == 1 || encoding == 2)
    {
        bool little_endian = (encoding == 2) ? false : true;
        uint32_t start_idx = 0;

        // Handle BOM for Encoding 1
        if (encoding == 1 && br >= 2) {
            if (buffer[0] == 0xFE && buffer[1] == 0xFF) {
                little_endian = false;
                start_idx = 2;
            } else if (buffer[0] == 0xFF && buffer[1] == 0xFE) {
                little_endian = true;
                start_idx = 2;
            }
        }

        for (uint32_t i = start_idx; i + 1 < br && oi < max_oi; i += 2)
        {
            uint16_t ch = little_endian ? (buffer[i] | (buffer[i+1] << 8)) 
                                        : ((buffer[i] << 8) | buffer[i+1]);
            if (ch == 0) break;

            if (ch < 0x80) {
                out[oi++] = (char)ch;
            } else if (ch < 0x800 && oi + 1 < max_oi) {
                out[oi++] = 0xC0 | (ch >> 6);
                out[oi++] = 0x80 | (ch & 0x3F);
            } else if (oi + 2 < max_oi) {
                out[oi++] = 0xE0 | (ch >> 12);
                out[oi++] = 0x80 | ((ch >> 6) & 0x3F);
                out[oi++] = 0x80 | (ch & 0x3F);
            }
        }
        out[oi] = '\0';
    }

    // Always seek to the end of the frame if it was larger than our buffer
    if (frame_size > br) {
        f_lseek(fil, f_tell(fil) + (frame_size - br));
    }
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

// Only fetch title, artist, album
void get_mp3_metadata_fast(const char *filename, track_info_t *track)
{
    // Initialize only filename, title, and artist
    strcpy(track->filename, filename);
    strcpy(track->title, "(unknown)");
    strcpy(track->artist, "(unknown)");
    strcpy(track->album, "(unknown)");

    FIL fil;
    UINT br;
    uint8_t header[10];
    uint8_t frame_header[10];

    if (f_open(&fil, filename, FA_READ) != FR_OK)
        return;

    // Check for ID3v2 header
    if (f_read(&fil, header, 10, &br) != FR_OK || br != 10 || memcmp(header, "ID3", 3) != 0)
    {
        f_close(&fil);
        return;
    }

    uint32_t tag_size = syncsafe_to_uint(&header[6]);
    uint32_t bytes_read = 0;

    // Iterate through frames
    while (bytes_read < tag_size)
    {
        if (f_read(&fil, frame_header, 10, &br) != FR_OK || br != 10)
            break;

        bytes_read += 10;
        
        if (frame_header[0] == 0) // Padding reached
            break;

        // Big-endian size conversion
        uint32_t size = (frame_header[4] << 24) | (frame_header[5] << 16) | 
                        (frame_header[6] << 8)  | frame_header[7];

        // Only process the three requested text frames
        if (memcmp(frame_header, "TIT2", 4) == 0) 
        {
            read_text_frame(&fil, size, track->title, sizeof(track->title));
        }
        else if (memcmp(frame_header, "TPE1", 4) == 0) 
        {
            read_text_frame(&fil, size, track->artist, sizeof(track->artist));
        }
        else if (memcmp(frame_header, "TALB", 4) == 0) 
        {
            read_text_frame(&fil, size, track->album, sizeof(track->album));
        }
        else 
        {
            // Quickly skip over everything else (including large APIC frames)
            f_lseek(&fil, f_tell(&fil) + size);
        }

        bytes_read += size;
    }

    f_close(&fil);
}

// Helper for qsort
int compare_filenames(const void *a, const void *b)
{
    const track_info_t *ta = (const track_info_t *)a;
    const track_info_t *tb = (const track_info_t *)b;
    return strcasecmp(ta->filename, tb->filename);
}

int compare_filenames_raw(const void *a, const void *b)
{
    return strcasecmp(a, b);
}

int compare_folders(const void *a, const void *b) {
    folder_info_t *folderA = (folder_info_t *)a;
    folder_info_t *folderB = (folder_info_t *)b;
    return strcasecmp(folderA->foldername, folderB->foldername);
}