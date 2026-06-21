import os
import io
import struct
from PIL import Image
from mutagen.mp3 import MP3
from mutagen.id3 import ID3, APIC

# --- CONFIGURATION ---
SOURCE_MUSIC_DIR = "/media/iamericmin/SONGS/"
OUTPUT_BIN = "./artwork.bin"
OUTPUT_LUT = "./artwork.lut"
TARGET_SIZE = 160
IMG_SIZE_BYTES = TARGET_SIZE * TARGET_SIZE * 2  # 51,200 bytes

# --- HASHING (64-bit FNV-1a) ---
def generate_fnv1a_64(artist, album, title):
    """
    Concatenates artist, album, and title, then computes a 64-bit FNV-1a hash.
    Outputs a standard 64-bit unsigned integer.
    """
    # Clean and concatenate strings smoothly
    combined_str = f"{artist.strip()}{album.strip()}{title.strip()}"
    
    fnv_offset_basis = 14695981039346656037
    fnv_prime = 1099511628211
    
    hash_value = fnv_offset_basis
    for byte in combined_str.encode('utf-8', 'ignore'):
        hash_value ^= byte
        hash_value = (hash_value * fnv_prime) & 0xFFFFFFFFFFFFFFFF
        
    return hash_value

# --- COMPRESSION (RGB888 to RGB565) ---
def convert_to_rgb565(r, g, b):
    """Converts 8-bit R, G, B channels into a single 16-bit RGB565 integer."""
    r_565 = (r >> 3) & 0x1F
    g_565 = (g >> 2) & 0x3F
    b_565 = (b >> 3) & 0x1F
    return (r_565 << 11) | (g_565 << 5) | b_565

def process_raw_image_data(raw_img_bytes, target_size=160):
    """Takes raw image bytes from an MP3, crops, resizes, and outputs RGB565 bytes."""
    with Image.open(io.BytesIO(raw_img_bytes)) as img:
        if img.mode != 'RGB':
            img = img.convert('RGB')
            
        # Center-crop to make it a perfect square
        width, height = img.size
        min_dim = min(width, height)
        left = (width - min_dim) / 2
        top = (height - min_dim) / 2
        right = (width + min_dim) / 2
        bottom = (height + min_dim) / 2
        img = img.crop((left, top, right, bottom))
        
        # Resize to target dimension
        img = img.resize((target_size, target_size), Image.Resampling.LANCZOS)
        
        binary_data = bytearray()
        pixels = img.getdata()
        
        for r, g, b in pixels:
            pixel_16 = convert_to_rgb565(r, g, b)
            # Big-Endian ('>H'). Change to '<H' if your DAP screen demands Little-Endian
            binary_data.extend(struct.pack('>H', pixel_16))
            
        return binary_data

# --- PIPELINE COMPILER ---
def build_artwork_cache_and_lut():
    """Scans music folder, extracts metadata + covers, and outputs .bin and sorted .lut"""
    if not os.path.exists(SOURCE_MUSIC_DIR):
        os.makedirs(SOURCE_MUSIC_DIR)
        print(f"\n[Info] Created '{SOURCE_MUSIC_DIR}' folder. Drop your MP3 files there and re-run!")
        return 0

    mp3_files = [f for f in os.listdir(SOURCE_MUSIC_DIR) if f.lower().endswith('.mp3')]
    
    if not mp3_files:
        print(f"\n[Warning] No MP3 files found in '{SOURCE_MUSIC_DIR}'.")
        return 0
        
    print(f"\n--- Phase 1: Processing MP3 Database & Extracting Images ---")
    print(f"Scanning {len(mp3_files)} MP3 tracks...")
    
    lut_entries = []
    valid_slots = 0
    
    with open(OUTPUT_BIN, 'wb') as bin_file:
        for filename in mp3_files:
            mp3_path = os.path.join(SOURCE_MUSIC_DIR, filename)
            try:
                # Load metadata tags using mutagen
                audio = MP3(mp3_path, ID3=ID3)
                
                # Fetch text metadata strings, fallback safely if tags are blank
                artist = str(audio.get('TPE1', 'Unknown Artist'))
                album = str(audio.get('TALB', 'Unknown Album'))
                title = str(audio.get('TIT2', 'Unknown Title'))
                
                # Look for the APIC (Attached Picture) frame
                apic_frame = None
                for key in audio.tags.keys():
                    if key.startswith('APIC:'):
                        apic_frame = audio.tags[key]
                        break
                
                if apic_frame is None:
                    print(f"  [Skipped] No embedded artwork found in: {filename}")
                    continue
                
                # Process the image and write to the binary cache block
                raw_rgb565_data = process_raw_image_data(apic_frame.data, TARGET_SIZE)
                bin_file.write(raw_rgb565_data)
                
                # Generate the tracking hash for this unique asset combination
                track_hash = generate_fnv1a_64(artist, album, title)
                
                # Queue entry data for our Look Up Table
                lut_entries.append({
                    'hash': track_hash,
                    'index': valid_slots
                })
                
                print(f"  Index {valid_slots:04d} -> Hash: {track_hash:20d} -> Title: \"{title}\"")
                valid_slots += 1
                
            except Exception as e:
                print(f"  [Error] Failed to process {filename}: {e}")
                
    if valid_slots == 0:
        print("\n[Warning] No artwork compiled. LUT compilation skipped.")
        return 0
        
    print(f"\n--- Phase 2: Generating Sorted Look-Up Table ({OUTPUT_LUT}) ---")
    
    # CRITICAL STEP: Sort the entries by hash value so your C binary-search works perfectly!
    lut_entries.sort(key=lambda x: x['hash'])
    
    with open(OUTPUT_LUT, 'wb') as lut_file:
        for entry in lut_entries:
            # Struct Packing Format: 
            # '>' = Big Endian (Change to '<' for Little Endian)
            # 'Q' = unsigned long long (64-bit / 8 bytes for our FNV hash)
            # 'I' = unsigned int (32-bit / 4 bytes for the raw index array reference pointer)
            # Total Size per Entry = 12 Bytes exactly
            packed_struct = struct.pack('<QI', entry['hash'], entry['index'])
            lut_file.write(packed_struct)
            
    print(f"Success! Generated {OUTPUT_BIN} ({os.path.getsize(OUTPUT_BIN)} bytes)")
    print(f"Success! Generated {OUTPUT_LUT} ({os.path.getsize(OUTPUT_LUT)} bytes) - Sorted and ready for binary search.")
    return valid_slots

# --- DECOMPRESSION (RGB565 to RGB888 for Verification) ---
def rgb565_to_rgb888(pixel_16):
    """Converts a 16-bit RGB565 integer back to 8-bit R, G, B channels."""
    r = ((pixel_16 >> 11) & 0x1F) << 3
    g = ((pixel_16 >> 5) & 0x3F) << 2
    b = (pixel_16 & 0x1F) << 3
    r |= (r >> 5)
    g |= (g >> 6)
    b |= (b >> 5)
    return (r, g, b)

def extract_and_verify(target_index, total_images):
    """Extracts a block from the binary file and reconstructs it back to a PNG."""
    if target_index >= total_images or target_index < 0:
        print(f"\n[Error] Index {target_index} is out of bounds! Valid range: 0 to {total_images - 1}")
        return
    
    offset = target_index * IMG_SIZE_BYTES
    
    with open(OUTPUT_BIN, 'rb') as f:
        f.seek(offset)
        raw_bytes = f.read(IMG_SIZE_BYTES)
        
    img = Image.new('RGB', (TARGET_SIZE, TARGET_SIZE))
    pixels = []
    
    for i in range(0, len(raw_bytes), 2):
        pixel_16 = struct.unpack('>H', raw_bytes[i:i+2])[0]
        pixels.append(rgb565_to_rgb888(pixel_16))
        
    img.putdata(pixels)
    output_filename = f"verified_index_{target_index}.png"
    img.save(output_filename)
    print(f"\n[Success] Extracted index {target_index} -> Saved to '{output_filename}'")

# --- MAIN SYSTEM EXECUTION ---
if __name__ == "__main__":
    # 1. Compile the dual-file systems
    total_slots = build_artwork_cache_and_lut()
    
    # 2. Enter interactive verification mode if images were generated
    if total_slots > 0:
        print(f"\n--- Phase 3: Interactive Verification ---")
        print(f"Loaded {total_slots} cached artwork slots from the newly built binary.")
        print("Type 'q' or 'exit' at any time to close the script.\n")
        
        while True:
            user_input = input(f"Enter an image index to verify (0 to {total_slots - 1}): ").strip()
            
            if user_input.lower() in ['q', 'exit']:
                print("Exiting pipeline toolkit.")
                break
                
            try:
                index = int(user_input)
                extract_and_verify(index, total_slots)
            except ValueError:
                print("[Error] Please enter a valid integer index.")
            print("-" * 50)