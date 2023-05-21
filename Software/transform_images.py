"""Transform images to use on Waveshare 7.3in 7-color E-paper display"""

import argparse
import logging
import math
from typing import Final, List

import PIL
import PIL.Image

FINAL_IMAGE_WIDTH: Final[int] = 800
FINAL_IMAGE_HEIGHT: Final[int] = 480
SCREEN_BACKGROUND_COLOR: Final[str] = "WHITE"

logging.basicConfig(format='%(asctime)s %(message)s', datefmt='%m/%d/%Y %I:%M:%S %p')
Logger = logging.getLogger(__name__)

# Palette colors taken from source code provided in
# https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT_(F)_Manual#Download_the_Program
Color_palette: List[int] = [
    0, 0, 0,                # Black
    255, 255, 255,          # White
    67, 138, 28,            # Green
    100, 64, 255,           # Blue
    191, 0, 0,              # Red
    255, 243, 56,           # Yellow
    232, 126, 0,            # Orange
    0, 0, 0                 # Black
]

def resize_image(original_image: PIL.Image.Image,
                 expected_width: int,
                 expected_height: int) -> PIL.Image.Image:
    """Resize original image to fit on the display screen.
    
       The original image is resized so that either:
       1. (image width == screen width) and (image height <= screen height), or
       2. (image width <= screen width) and (image height == screen height)
    """
    width, height = original_image.size

    width_scale_factor = (1.0 * width) / expected_width
    Logger.debug(f"Width scale factor {width_scale_factor}")

    height_scale_factor = (1.0 * height) / expected_height
    Logger.debug(f"Height scale factor {height_scale_factor}")

    scale_factor = min(width_scale_factor, height_scale_factor)
    Logger.debug(f"Chosen scale factor {scale_factor}")

    new_width = min(math.floor(scale_factor * width), expected_width)
    new_height = min(math.floor(scale_factor * height), expected_height)
    Logger.debug(f"New size ({new_width}, {new_height})")

    resized_image = original_image.resize((new_width, new_height), resample=PIL.Image.Resampling.LANCZOS)

    return resized_image

def create_screen_sized_image(original_image: PIL.Image.Image,
                              screen_width: int,
                              screen_height: int) -> PIL.Image.Image:
    """Overlay the original image on given background to fit screen.
    
       In case the image is smaller than screen size, overlay it in given
       background color so that the new image has exactly the same dimensions
       as the screen
    """
    screen_image = PIL.Image.new('RGB', (screen_width, screen_height), color=SCREEN_BACKGROUND_COLOR)
    screen_image.paste(original_image)
    return screen_image

# Implementation derived from https://stackoverflow.com/a/29438149
def convert_image_palette(original_image: PIL.Image.Image,
                          palette: List[int],
                          with_dithering: bool) -> PIL.Image.Image:
    """Convert the image colors to palette used by the display."""
    temp_image = PIL.Image.new('P', (16, 16))
    # Assert that palette is of RGB format
    assert(len(palette) % 3 == 0)
    palette_color_count = len(palette)//3
    # Copy the palette colors multiple times to expand up to 256 colors
    temp_image.putpalette(palette*(256//palette_color_count))
    dithering_option = PIL.Image.Dither.FLOYDSTEINBERG if with_dithering else PIL.Image.Dither.NONE
    Logger.debug(f"Applying dithering: {dithering_option}")
    converted_image = original_image.quantize(palette=temp_image, dither=dithering_option)
    return converted_image

def save_converted_image(converted_image: PIL.Image.Image,
                         filepath: str) -> None:
    """Store the processed image as sequence of bytes in a file.
    
       The format used is specific to the display being used. Current
       implementation used the format mentioned at:
       https://www.waveshare.com/wiki/7.3inch_e-Paper_HAT_(F)_Manual#Programming_Principles
    """
    width, height = converted_image.size
    assert(width%2 == 0)
    with open(filepath, "wb") as f:
        for y in range(height):
            for x in range(width//2):
                pixel_color_1 = converted_image.getpixel((2*x, y))
                pixel_color_2 = converted_image.getpixel((2*x+1, y))
                f.write(((pixel_color_1 << 4) | pixel_color_2).to_bytes(1, byteorder="little"))

def transform_and_save_image(input_filepath: str,
                             output_filepath: str,
                             with_dithering: bool,
                             show_processed_image: bool) -> None:
    """Main function to transform user provided image into sequence of bytes to display on screen."""
    original_image = PIL.Image.open(input_filepath)
    Logger.debug(f"Opened image at path {input_filepath}")
    Logger.debug(f"Image format: {original_image.format}")
    Logger.debug(f"Image size: {original_image.size}")
    Logger.debug(f"Image mode: {original_image.mode}")

    resized_image = resize_image(original_image, FINAL_IMAGE_WIDTH, FINAL_IMAGE_HEIGHT)
    Logger.debug(f"Image resize to {resized_image.size}")

    screen_image = create_screen_sized_image(resized_image, FINAL_IMAGE_WIDTH, FINAL_IMAGE_HEIGHT)
    Logger.debug(f"Created screen sized image with size {screen_image.size}")

    converted_image = convert_image_palette(screen_image, Color_palette, with_dithering)
    Logger.debug("Converted image to use provided color palette")

    save_converted_image(converted_image, output_filepath)

    if show_processed_image:
        converted_image.show()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input_filepath", help="Path to image file to convert")
    parser.add_argument("output_filepath", help="Path where the output image should be stored")
    parser.add_argument("--show-processed-image", help="Show the final image prepared for display",
                        action="store_true")
    parser.add_argument("--with-dithering", help=("Apply dithering when reducing colorspace (palette)"
                                                  "This may be helpful with portraits"), action="store_true")
    parser.add_argument("--debug", help="Print debug logs", action="store_true")
    args = parser.parse_args()
    if (args.debug):
        Logger.setLevel(logging.DEBUG)
    transform_and_save_image(args.input_filepath, args.output_filepath, args.with_dithering,
                             args.show_processed_image)