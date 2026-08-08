import os
from PIL import Image, ImageFont, ImageDraw

# ---------------------------------------------------------
# 1. Configuration
# ---------------------------------------------------------
TTF_FONT_FILE = 'CPMono_Bold.ttf'  
FONT_POINT_SIZE = 18  # Font size inside canvas

# ---------------------------------------------------------
# TWEAK SMOOTHING & BLOCKINESS HERE:
# ---------------------------------------------------------
# SMOOTHING_THRESHOLD controls the brightness roll-off (0-255).
# - Lower (30 to 80)   = Thicker, blockier, mostly solid white with sharp edge drop-off.
# - Higher (150 to 220) = Thinner, softer, gentler feathering/anti-aliasing.
# - Default Pillow behavior is roughly equivalent to 180-200.
SMOOTHING_THRESHOLD = 120

# Optional: Physical stroke outline (0 = off, 1 = adds 1px extra weight to stems)
STROKE_WIDTH = 0  

OUTPUT_C_FILE = '../lib/font/font.c'
OUTPUT_H_FILE = '../lib/font/font.h'

GRID_WIDTH = 11   # Character width in pixels
GRID_HEIGHT = 20  # Character height in pixels

# Full ASCII spectrum (space through tilde)
CHARACTERS = ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'

# ---------------------------------------------------------
# 2. Render TTF with Custom Roll-Off Adjustment
# ---------------------------------------------------------
def adjust_rolloff(val):
    """Remaps raw Pillow alpha values to adjust smoothing harshness."""
    bg_cutoff = 15  # Keeps background clean pure black (0x00)
    
    if val < bg_cutoff: 
        return 0
    elif val > SMOOTHING_THRESHOLD: 
        return 255  # Force mid-tones up to full brightness (255) for chunky text
    else:
        # Scale the tight transition zone between 0x00 and 0xFF
        return int(((val - bg_cutoff) / (SMOOTHING_THRESHOLD - bg_cutoff)) * 255)

def render_ttf_fonts():
    if not os.path.exists(TTF_FONT_FILE):
        raise FileNotFoundError(f"Font file '{TTF_FONT_FILE}' not found. Check the file path.")

    ttf_font = ImageFont.truetype(TTF_FONT_FILE, FONT_POINT_SIZE)
    pixel_data = []

    # Get font-wide metrics
    ascent, descent = ttf_font.getmetrics()
    
    # Calculate fixed baseline
    line_height = ascent + descent
    baseline_y = (GRID_HEIGHT - line_height) // 2 + ascent

    for char in CHARACTERS:
        img = Image.new('L', (GRID_WIDTH, GRID_HEIGHT), color=0)
        draw = ImageDraw.Draw(img)

        # Calculate horizontal centering
        bbox = draw.textbbox((0, 0), char, font=ttf_font)
        text_w = bbox[2] - bbox[0]
        offset_x = max(0, (GRID_WIDTH - text_w) // 2 - bbox[0])

        # Draw glyph with optional physical stroke weight
        draw.text(
            (offset_x, baseline_y), 
            char, 
            font=ttf_font, 
            fill=255, 
            anchor="ls",
            stroke_width=STROKE_WIDTH,
            stroke_fill=255 if STROKE_WIDTH > 0 else None
        )

        # Apply the contrast curve / roll-off modification
        img = img.point(adjust_rolloff)

        # Extract 0x00-0xFF alpha values
        px = img.load()
        char_px = []
        for y in range(GRID_HEIGHT):
            for x in range(GRID_WIDTH):
                char_px.append(px[x, y])
                
        pixel_data.append(char_px)

    return pixel_data

# ---------------------------------------------------------
# 3. C File Generation Functions
# ---------------------------------------------------------
def write_header():
    with open(OUTPUT_H_FILE, 'w') as f:
        f.write("#ifndef FONT_H\n")
        f.write("#define FONT_H\n\n")
        f.write("#include <stdint.h>\n")
        f.write("#include <stddef.h>\n\n")
        f.write("extern const uint8_t font_width;\n")
        f.write("extern const uint8_t font_height;\n\n")
        f.write("struct Font {\n")
        f.write("    char letter;\n")
        f.write(f"    uint8_t code[{GRID_WIDTH * GRID_HEIGHT}]; // 8-bit alpha intensity map (0-255)\n")
        f.write("};\n\n")
        f.write("extern const struct Font font[];\n")
        f.write("const struct Font* find_font_char(char c);\n\n")
        f.write("#endif // FONT_H\n")

def write_source(pixel_data):
    os.makedirs(os.path.dirname(OUTPUT_C_FILE), exist_ok=True)
    
    with open(OUTPUT_C_FILE, 'w') as f:
        f.write('#include "font.h"\n\n')
        f.write(f"const uint8_t font_width = {GRID_WIDTH};\n")
        f.write(f"const uint8_t font_height = {GRID_HEIGHT};\n\n")
        f.write("const struct Font font[] = {\n")

        for idx, pixels in enumerate(pixel_data):
            char = CHARACTERS[idx]
            escaped_char = "\\'" if char == "'" else ("\\\\" if char == "\\" else char)

            f.write(f"  {{ '{escaped_char}', {{\n")

            for i, val in enumerate(pixels):
                if i % GRID_WIDTH == 0:
                    f.write("    ")
                f.write(f"0x{val:02X}")
                if i != len(pixels) - 1:
                    f.write(",")
                if (i + 1) % GRID_WIDTH == 0:
                    f.write("\n")

            f.write("  }}")
            if idx != len(pixel_data) - 1:
                f.write(",\n")
            else:
                f.write("\n")

        f.write("};\n\n")

        f.write("const struct Font* find_font_char(char c) {\n")
        f.write("    size_t font_count = sizeof(font) / sizeof(font[0]);\n")
        f.write("    for (size_t i = 0; i < font_count; i++) {\n")
        f.write("        if (font[i].letter == c) return &font[i];\n")
        f.write("    }\n")
        f.write("    return NULL;\n")
        f.write("}\n")

if __name__ == '__main__':
    print(f"Generating font from '{TTF_FONT_FILE}' (Threshold={SMOOTHING_THRESHOLD})...")
    data = render_ttf_fonts()
    write_header()
    write_source(data)
    print("Done! Updated font.h and font.c generated successfully.")