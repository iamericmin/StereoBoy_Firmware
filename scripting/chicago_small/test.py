from PIL import Image



i = Image.open("chicago_small.png")
with open("chicago_small.fnt", 'r') as f:
    for i in range(4): 
        f.readline()
    for l in f:
        
        print(l, end='')
