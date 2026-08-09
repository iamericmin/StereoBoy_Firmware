#include "album_art.h"
#include <stdint.h>
#include <string.h>

#define IMG_WIDTH  160
#define IMG_HEIGHT 160
#define IMG_BYTES 160 * 160 * 2

// generate FNV-1a hash for LUT lookup
#include <ctype.h>

// Helper function to trim leading/trailing whitespace (matches Python's .strip())
const char* trim_string(const char* str, size_t* out_len) {
    while (isspace((unsigned char)*str)) str++; // Trim leading
    
    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) len--; // Trim trailing
    
    *out_len = len;
    return str;
}

uint64_t generate_FNV(const char* artist, const char* album, const char* title) {
    uint64_t hash = 14695981039346656037ULL;
    uint64_t prime = 1099511628211ULL;

    size_t artist_len = 0, album_len = 0, title_len = 0;
    
    // Trim strings exactly like Python's .strip()
    const char* clean_artist = trim_string(artist, &artist_len);
    const char* clean_album  = trim_string(album, &album_len);
    const char* clean_title  = trim_string(title, &title_len);

    // 1. Hash Clean Artist
    for (size_t i = 0; i < artist_len; i++) {
        hash ^= (uint8_t)clean_artist[i];
        hash *= prime;
    }
    
    // 2. Hash Clean Album
    for (size_t i = 0; i < album_len; i++) {
        hash ^= (uint8_t)clean_album[i];
        hash *= prime;
    }
    
    // 3. Hash Clean Title
    for (size_t i = 0; i < title_len; i++) {
        hash ^= (uint8_t)clean_title[i];
        hash *= prime;
    }

    return hash;
}

bool load_LUT() {
    FIL file;
    FRESULT fr;
    UINT bytes_read;

    // 1. Open the file from the root directory ("/" or "0:/")
    fr = f_open(&file, "/artwork.lut", FA_READ);
    if (fr != FR_OK) {
        printf("Error: Could not open artwork.lut (Code: %d)\n", fr);
        return false;
    }

    // 2. Determine file size and calculate total entries
    DWORD file_size = f_size(&file);
    lut_entry_count = file_size / sizeof(LUT_entry_t);

    if (lut_entry_count == 0) {
        f_close(&file);
        return false;
    }

    // 3. Allocate a precise chunk of RAM heap memory for the array
    // 5000 tracks * 12 bytes = ~60KB allocated dynamically
    artCache_LUT = (LUT_entry_t*)malloc(file_size);
    if (artCache_LUT == NULL) {
        printf("Error: Out of RAM! Could not allocate memory for LUT.\n");
        f_close(&file);
        return false;
    }

    // 4. Read the ENTIRE file into RAM in one fast block operation
    fr = f_read(&file, artCache_LUT, file_size, &bytes_read);
    
    // Close the file handle since we don't need the SD card for lookups anymore
    f_close(&file);

    if (fr == FR_OK && bytes_read == file_size) {
        printf("Successfully loaded %lu LUT entries into RAM (%lu bytes).\n", lut_entry_count, file_size);
        // Insert this right after a successful f_read:
        printf("\n--- Printing %lu RAM LUT Entries ---\n", lut_entry_count);
        for (uint32_t i = 0; i < lut_entry_count; i++) {
            printf("Entry [%04lu] -> Hash (Dec): %llu | Hash (Hex): 0x%016llX | Image Index: %lu\n", 
                i, 
                artCache_LUT[i].hash, 
                artCache_LUT[i].hash, 
                artCache_LUT[i].pointer);
        }
        printf("------------------------------------\n\n");
        return true;
    } else {
        printf("Error reading LUT file data.\n");
        free(artCache_LUT); // Free memory if reading failed
        artCache_LUT = NULL;
        return false;
    }    
}

/**
 * Looks up a 64-bit FNV-1a track hash in the RAM-cached LUT using a Binary Search.
 * * @param hash The 64-bit hash calculated from the currently playing MP3's metadata.
 * @return The 32-bit image index if found; returns 0xFFFFFFFF if no artwork exists.
 */
uint32_t lookup_LUT(uint64_t hash) {
    // Safety check: Ensure the LUT was actually loaded into RAM successfully
    if (artCache_LUT == NULL || lut_entry_count == 0) {
        return 0xFFFFFFFF; 
    }

    int low = 0;
    int high = lut_entry_count - 1;

    while (low <= high) {
        // Calculate the middle index safely to prevent potential integer overflow
        int mid = low + (high - low) / 2;

        // Check if the middle entry is our target hash
        if (artCache_LUT[mid].hash == hash) {
            printf("Hash match found! Hash (Hex): 0x%016llX | Image Index: %lu\n", artCache_LUT[mid].hash, artCache_LUT[mid].pointer);
            return artCache_LUT[mid].pointer; // Success! Return the artwork index
        }

        // If our target hash is larger, ignore the lower half
        if (artCache_LUT[mid].hash < hash) {
            low = mid + 1;
        } 
        // If our target hash is smaller, ignore the upper half
        else {
            high = mid - 1;
        }
    }

    // Hash not found in the LUT file (e.g., song has no embedded album art)
    return 0xFFFFFFFF; 
}

/**
 * Seeks to the specified artwork index in artwork.bin and reads the raw 
 * RGB565 data straight into an external 160x160 pixel buffer.
 *
 * @param img_buffer Pointer to a uint16_t array large enough to hold 25,600 elements (160x160).
 * @param pointer The raw index returned from your lookup_LUT() function.
 * @return true if the image was successfully loaded; false if an error occurred.
 */
bool load_album_cover(uint16_t *img_buffer, uint32_t pointer) {
    // Safety check: Ensure we aren't passing a null pointer or an invalid index
    if (img_buffer == NULL || pointer == 0xFFFFFFFF) {
        return false;
    }

    FIL file;
    FRESULT fr;
    UINT bytes_read;

    // Define dimensions matching your structural profile
    const uint32_t img_width = 160;
    const uint32_t img_height = 160;
    const uint32_t bytes_per_pixel = 2; // RGB565 takes 2 bytes
    const uint32_t total_image_bytes = img_width * img_height * bytes_per_pixel; // 51,200 bytes

    // 1. Open the file from the root directory
    fr = f_open(&file, "/artwork.bin", FA_READ);
    if (fr != FR_OK) {
        printf("Error: Could not open artwork.bin (Code: %d)\n", fr);
        // Missing file or SD card unmounted
        return false; 
    }

    // 2. Calculate the exact math boundary offset
    FSIZE_t target_byte_offset = (FSIZE_t)pointer * total_image_bytes;

    // 3. Move the file pointer directly to the target image data block
    fr = f_lseek(&file, target_byte_offset);
    if (fr != FR_OK) {
        f_close(&file);
        return false; // Seek failed (e.g., index out of bounds of actual file size)
    }

    // 4. Stream the raw binary block out of the SD card and straight into RAM
    fr = f_read(&file, img_buffer, total_image_bytes, &bytes_read);
    
    // Always close the file handle to prevent memory leaks and file system corruption
    f_close(&file);

    // Verify everything transferred cleanly without breaking sector boundaries
    if (fr == FR_OK && bytes_read == total_image_bytes) {
        printf("artwork loaded!\n");
        return true; // Array populated with perfect 16-bit RGB565 pixels!
    } else {
        // Change your error print to capture the 'fr' variable:
        printf("[Art Error] Hash found, but f_read failed! FatFs Code: %d\n", fr);
    }

    return false; // Read operation failed mid-stream
}

int display_album_art(uint16_t *img_buffer, const char* artist, const char* album, const char* title) {
    memset(frame_buffer, 0, sizeof(frame_buffer));

    // 1. Calculate the hash right here locally
    uint64_t runtime_hash = generate_FNV(artist, album, title);
    
    // 2. Perform the lookup
    uint32_t pointer = lookup_LUT(runtime_hash);
    
    printf("[Art Debug] Hashing: Target=\"%s%s%s\"\n", artist, album, title);
    printf("[Art Debug] Generated Hash: 0x%016llX -> Lookup Result: %lu\n", runtime_hash, pointer);
    
    if (pointer == 0xFFFFFFFF) {
        printf("[Art Error] Hash was NOT found in the RAM LUT table!\n");
        return -1;
    }

    if (load_album_cover(img_buffer, pointer)) {
        const int offset = (SCREEN_WIDTH - 160) / 2;
        for (int y = 0; y < 160; y++)
        {
            uint16_t *dst = &frame_buffer[(y + offset) * SCREEN_WIDTH + offset];
            uint16_t *src = &img_buffer[y * 160];
            memcpy(dst, src, 160 * sizeof(uint16_t));
        }
        printf("[Art Success] Pixels copied to frame buffer cleanly.\n");
        return 0;
    } else {
        printf("[Art Error] Hash found, but reading 51200 bytes from artwork.bin failed!\n");
        return -1;
    }
}

bool load_album_cover_by_index(uint16_t *img_buffer, uint32_t global_track_idx) {
    if (img_buffer == NULL) return false;

    FIL file;
    FRESULT fr;
    UINT bytes_read;

    // 1. Open the actual filename output by Python
    fr = f_open(&file, "0:/.artwork.sbc", FA_READ);
    if (fr != FR_OK) {
        printf("[Art Error] Could not open 0:/.artwork.sbc (FatFs Code: %d)\n", fr);
        return false; 
    }

    // 2. Compute byte offset (Index * 51,200)
    FSIZE_t target_offset = (FSIZE_t)global_track_idx * IMG_BYTES;

    // 3. Seek to offset
    fr = f_lseek(&file, target_offset);
    if (fr != FR_OK) {
        printf("[Art Error] Seek failed to offset %llu (Code: %d)\n", target_offset, fr);
        f_close(&file);
        return false;
    }

    // 4. Read RGB565 block into buffer
    fr = f_read(&file, img_buffer, IMG_BYTES, &bytes_read);
    f_close(&file);

    if (fr == FR_OK && bytes_read == IMG_BYTES) {
        return true; 
    }

    printf("[Art Error] Read failed! Read %u / %d bytes (Code: %d)\n", bytes_read, IMG_BYTES, fr);
    return false;
}

/**
 * Renders the 160x160 artwork centered onto the 240x240 display frame buffer.
 */
int display_album_art_by_index(uint16_t *img_buffer, uint32_t global_track_idx) {

    if (load_album_cover_by_index(img_buffer, global_track_idx)) {
        // Clear framebuffer first
        memset(frame_buffer, 0, sizeof(frame_buffer));
        // Center 160x160 inside 240x240 screen (offset = 40)
        const int offset = (240 - 160) / 2; // 40 px margin
        
        for (int y = 0; y < 160; y++) {
            uint16_t *dst = &frame_buffer[(y + offset) * 240 + offset];
            uint16_t *src = &img_buffer[y * 160];
            memcpy(dst, src, 160 * sizeof(uint16_t));
        }
        
        printf("[Art Success] Displayed art for Global Track ID: %lu\n", global_track_idx);
        
        return 0;
    } else {
        return -1;
    }
}