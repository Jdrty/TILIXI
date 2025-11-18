from PIL import Image

# absolute paths
input_file = "/home/julian/school/ACES/ISP/TILIXI/project/assets/icon.jpg"
output_file = "/home/julian/school/ACES/ISP/TILIXI/project/data/logo.raw"

# open image and convert to RGB
img = Image.open(input_file).convert("RGB")
width, height = img.size

# write RGB565 raw file
with open(output_file, "wb") as f:
    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            f.write(rgb565.to_bytes(2, "big"))

