import os
import struct
from PIL import Image

def rgb565_to_rgb888(pixel_16):
    """Converts a 16-bit RGB565 integer back to 8-bit R, G, B channels."""
    r = ((pixel_16 >> 11) & 0x1F) << 3
    g = ((pixel_16 >> 5) & 0x3F) << 2
    b = (pixel_16 & 0x1F) << 3
    
    # Handle slight math loss by copying highest bits to lowest bits
    r |= (r >> 5)
    g |= (g >> 6)
    b |= (b >> 5)
    
    return (r, g, b)

def extract_and_verify(bin_path, target_index, target_size=160):
    img_size_bytes = target_size * target_size * 2 # 51,200 bytes
    offset = target_index * img_size_bytes
    
    # Check total file size to see how many images exist
    file_size = os.path.getsize(bin_path)
    total_images = file_size // img_size_bytes
    
    if target_index >= total_images or target_index < 0:
        print(f"\n[Error] Index {target_index} is out of bounds!")
        print(f"        Your file has valid indices from 0 to {total_images - 1}.")
        return False
    
    with open(bin_path, 'rb') as f:
        f.seek(offset)
        raw_bytes = f.read(img_size_bytes)
        
    img = Image.new('RGB', (target_size, target_size))
    pixels = []
    
    # Unpack bytes (Change '>H' to '<H' if your generator script used Little-Endian)
    for i in range(0, len(raw_bytes), 2):
        pixel_16 = struct.unpack('>H', raw_bytes[i:i+2])[0]
        pixels.append(rgb565_to_rgb888(pixel_16))
        
    img.putdata(pixels)
    output_filename = f"verified_index_{target_index}.png"
    img.save(output_filename)
    print(f"\n[Success] Reconstructed index {target_index} -> Saved to '{output_filename}'")
    return True

# --- Interactive Execution Loop ---
if __name__ == "__main__":
    BIN_FILE = "./artwork.bin"
    
    if not os.path.exists(BIN_FILE):
        print(f"Error: Could not find '{BIN_FILE}'. Please generate it first.")
        exit(1)
        
    # Calculate how many images are currently packed inside the bin file
    img_size_bytes = 160 * 160 * 2
    total_images = os.path.getsize(BIN_FILE) // img_size_bytes
    print(f"Loaded '{BIN_FILE}' containing {total_images} cached artwork slots.")
    print("Type 'q' or 'exit' at any time to quit.\n")
    
    while True:
        user_input = input(f"Enter an image index to extract (0 to {total_images - 1}): ").strip()
        
        if user_input.lower() in ['q', 'exit']:
            print("Exiting checker script.")
            break
            
        try:
            index = int(user_input)
            extract_and_verify(BIN_FILE, target_index=index)
        except ValueError:
            print("[Error] Please enter a valid integer number.")
        print("-" * 50)