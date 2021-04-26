// wfctool
//
// Author:  Krystian Samp (samp.krystian at gmail.component)
// License: MIT
// Version: 0.01
//
// Wave Function Collapse command-line tool based on khuwfc. The tool
// can be used to generate WFC images.
//
// COMPILING
// =============================================================================
//
// wfctool depends on stb_image.h and stb_write.h. Place both files in the
// same directory as wfctool.c.
//
//         make
//         ./wfc
//
// Run ./wfc to see available options
//
//
// Basic usage:
//
//         ./wfc -m overlapping -w 128 -h 128 input.png output.png
//
//
// THANKS
// =============================================================================
//
// Thanks for using wfctool. If you find any bugs, have questions, or miss
// a feature please let me know. Also, if you'd like to share your works
// it's very appreciated. Please use my email at the top of the file.
//

#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#define WFC_IMPLEMENTATION
#define WFC_USE_STB
#include "wfc.h"

void print_summary(struct wfc *wfc, const char *input_image, const char *output_image)
{
  printf("\n");
  printf("method:               %s\n", wfc->method == WFC_METHOD_OVERLAPPING ? "overlapping" : "tiled");
  printf("seed:                 %u\n\n", wfc->seed);
  printf("input image:          %s\n", input_image);
  printf("input size:           %dx%d\n", wfc->image->width, wfc->image->height);
  printf("input components:     %d\n", wfc->image->component_cnt);
  printf("tile size:            %dx%d\n", wfc->tile_width, wfc->tile_height);
  printf("expand input:         %d\n", wfc->expand_input);
  printf("xflip tiles:          %d\n", wfc->xflip_tiles);
  printf("yflip tiles:          %d\n", wfc->yflip_tiles);
  printf("rotate tiles:         %d\n", wfc->rotate_tiles);
  printf("tile count:           %d\n", wfc->tile_cnt);
  printf("\n");
  printf("output image:         %s\n", output_image);
  printf("output size:          %dx%d\n", wfc->output_width, wfc->output_height);
  printf("cell count:           %d\n", wfc->cell_cnt);
  printf("\n");
}

void usage(const char *program_name, int exit_code)
{
  if (exit_code != EXIT_SUCCESS) {
    printf("Wrong input\n\n");
  }

  printf("\
Wave Function Collapse image generator.\
\n\n\
");
  printf("\
Usage:\n\
  %s -m METHOD [OPTIONS] input_image output_image\n\n", program_name);
  printf("\
Following options are available:\n\n\
  -m METHOD, --method=METHOD          Must be 'overlapping'\n\
  -w num, --width=num                 Output width in pixels\n\
  -h num, --height=num                Output height in pixels\n\
  -W num, --tile-width=num            Tile width in pixels\n\
  -H num, --tile-height=num           Tile height in pixels\n\
  -e 0|1, --expand-image=0|1          Wrap input image on right and bottom\n\
  -x 0|1, --xflip=0|1                 Add horizontal flips of all tiles\n\
  -y 0|1, --yflip=0|1                 Add vertical flips of all tiles\n\
  -r 0|1, --rotate=0|1                Add n*90deg rotations of all tiles\n\
\n\
");

  printf("\
The only supported METHOD at the moment is the 'overlapping' method.\n\n");

  printf("\
Example:\n\
  ./wfc -m overlapping -w 128 -h 128 -W 3 -H 3 -e 1 -x 1 -y 1 -r 1 plant.png output.png\n\n\
");

  exit(exit_code);
}

int arg_method(int argc, const char **argv, int *i, enum wfc__method *method)
{
  if (strcmp(argv[*i], "-m")==0) {
    (*i)++;
    if (*i==argc || strcmp(argv[*i], "overlapping")!=0) {
      usage(argv[0], EXIT_FAILURE);
    }
    (*i)++;
    *method = WFC_METHOD_OVERLAPPING;
    return 0;
  } else if (strcmp(argv[*i], "--method=overlapping")==0) {
    *method = WFC_METHOD_OVERLAPPING;
    (*i)++;
    return 0;
  }

  return -1;
}

int arg_num(int argc, const char **argv, int *i, const char *short_name, const char *long_name, int *num)
{
  int n;
  char str[128];

  sprintf(str, "-%s", short_name);
  if (strcmp(argv[*i], str)==0) {
    (*i)++;
    if (*i==argc) {
      usage(argv[0], EXIT_FAILURE);
    }
    if (sscanf(argv[*i], "%d", &n) != 1) {
      usage(argv[0], EXIT_FAILURE);
    }
    (*i)++;
    *num = n;
    return 0;
  } else {
    sprintf(str, "--%s=%%d", long_name);
    if (sscanf(argv[*i], str, &n) != 1) {
      return -1;
    }
    (*i)++;
    *num = n;
    return 0;
  }
}

// Can terminate the program if the arguments are incorrect
void read_args(int argc, const char **argv, enum wfc__method *method, const char **input, const char **output, int *width, int *height, int *tile_width, int *tile_height, int *expand_image, int *xflip_tiles, int *yflip_tiles, int *rotate_tiles)
{
  if (argc<2) {
    usage(argv[0], EXIT_FAILURE);
  }

  *method = 99999;

  int i=1;
  while (i<argc) {
    if (arg_method(argc, argv, &i, method) == 0) continue;
    if (arg_num(argc, argv, &i, "w", "width", width) == 0) continue;
    if (arg_num(argc, argv, &i, "h", "height", height) == 0) continue;
    if (arg_num(argc, argv, &i, "W", "tile-width", tile_width) == 0) continue;
    if (arg_num(argc, argv, &i, "H", "tile-height", tile_height) == 0) continue;
    if (arg_num(argc, argv, &i, "e", "expand-image", expand_image) == 0) continue;
    if (arg_num(argc, argv, &i, "x", "xflip", xflip_tiles) == 0) continue;
    if (arg_num(argc, argv, &i, "y", "yflip", yflip_tiles) == 0) continue;
    if (arg_num(argc, argv, &i, "r", "rotate", rotate_tiles) == 0) continue;

    if (i != argc-2)
      usage(argv[0], EXIT_FAILURE);

    if (*method == 99999)
      usage(argv[0], EXIT_FAILURE);

    *input = argv[i];
    *output = argv[i+1];
    return;
  };

}

int main(int argc, const char **argv)
{
  enum wfc__method method;
  const char *input_filename;
  const char *output_filename;
  int output_width = 128;
  int output_height = 128;
  int tile_width = 3;
  int tile_height = 3;
  int expand_input = 1;
  int xflip_tiles = 1;
  int yflip_tiles = 1;
  int rotate_tiles = 1;

  read_args(argc,
            argv,
            &method,
            &input_filename,
            &output_filename,
            &output_width,
            &output_height,
            &tile_width,
            &tile_height,
            &expand_input,
            &xflip_tiles,
            &yflip_tiles,
            &rotate_tiles);

  struct wfc_image *image = wfc_img_load(input_filename);
  if (image == NULL) {
    p("Error: cannot load image: %s\n", input_filename);
    return EXIT_FAILURE;
  }

  struct wfc *wfc = wfc_overlapping(output_width,
                                    output_height,
                                    image,
                                    tile_width,
                                    tile_height,
                                    expand_input,
                                    xflip_tiles,
                                    yflip_tiles,
                                    rotate_tiles);

  if (wfc == NULL) {
    p("Error: cannot create wfc\n");
    goto CLEANUP;
  }

  /* wfc_export_tiles(wfc, "tmp"); */
  print_summary(wfc, input_filename, output_filename);

  if (!wfc_run(wfc, -1)) {
    p("Contradiction occurred, try again\n");
    goto CLEANUP;
  }

  if (!wfc_export(wfc, output_filename)) {
    p("Error: cannot save image: %s\n", output_filename);
    goto CLEANUP;
  }

  wfc_img_destroy(image);
  wfc_destroy(wfc);
  return EXIT_SUCCESS;

 CLEANUP:
  wfc_img_destroy(image);
  wfc_destroy(wfc);
  return EXIT_FAILURE;
}

/*******************************************************************************
  LICENSE:

  the MIT License (MIT)

  Copyright (c) 2020 Krystian Samp

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.


  DISCLAIMER:

  This software is supplied "AS IS" without any warranties and support
*******************************************************************************/
