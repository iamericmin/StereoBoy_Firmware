import PIL
from PIL import Image
from math import floor

'''
Use this website to generate bitmaps:
https://stmn.itch.io/font2bitmap
'''

font_file = 'a_cpmono.png'

characters = ' !"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~'
# characters = 'A'
characters = [c for c in characters]

chars_per_row = 19
grid_width = 11
grid_height = 20

def genSubImages(filename):
    sub_ims = []
    with Image.open(filename) as im:
        for i in range(len(characters)):
            coords = (grid_width*(i%19), grid_height*(floor(i/19)), grid_width*(i%19 + 1), grid_height * (1 + floor(i/19)))
            sub_im = im.crop(coords)
            im.save("a.png")
            sub_ims.append(sub_im)
    return sub_ims

def parsePixels(sub_ims):
    px_colors = []
    for s in sub_ims:
        # print(type(s))
        assert (type(s) == PIL.Image.Image)
        char_px = []
        px = s.load()
        for i in range(grid_height):
            for j in range(grid_width):
                color = px[j,i]
                print(color)
                # color = (color[0], color[1], color[2])
                color_sum = (color[0] + color[1] + color[2])
                
                char_px.append(1 if color_sum == 255*3 else 0)
        px_colors.append(char_px)
    return px_colors

if __name__ == '__main__':
    sub_ims = genSubImages(font_file)
    pixel_tupels = parsePixels(sub_ims)
    with open('output.c', 'w+') as f:
        f.write(f'#include "font.h"\n')
        f.write(f'#include "stddef.h"\n\n')
        # f.write(f"int font_width = {grid_width};\n")
        # f.write(f"int font_height = {grid_height};\n\n")

        f.write("struct Font font[] = {\n")

        for idx, pixels in enumerate(pixel_tupels):
            f.write(f"{{'")
            if characters[idx] == "'" or characters[idx] == "\\":
                f.write(f"\\{characters[idx]}")
            else:
                f.write(f"{characters[idx]}")
            f.write(f"', {{")
                    
            for i, p in enumerate(pixels):
                if i % grid_width == 0:
                    f.write("\n")
                # if len(p) != 6:
                    # p = f"0x{''.join(['0' for j in range(6-len(p))])}{p[2:]}"
                    # p = f"0" 
                f.write(f"{str(p)}")
                if (i != grid_width*grid_height-1):
                    f.write(",")
            f.write(f"}}}}")
            if idx != len(pixel_tupels)-1:
                f.write(f",\n")
            else:
                f.write(f"}};")

        f.write("\n\n")
        # f.write("const struct Font* find_font_char(char c) {\n")
        # f.write("\tfor (int i = 0; font[i].letter != 0; i++) {\n")
        # f.write("\t\tif (font[i].letter == c) return &font[i];\n")
        # f.write("\t}\n")
        # f.write("\treturn NULL;\n")
        # f.write("}\n")