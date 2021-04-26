# khu_wfc

Single-file Wave Function Collapse library in C, plus command-line tool

- License: MIT
- Version: 0.01

This is an early version that supports the overlapping WFC method.
The method takes an input image and generates output image which is
locally similar to the input image. Here're a few examples of
input/output pairs:

<img width="950px" src="https://github.com/krychu/wfc/blob/master/samples/mosaic.png?raw=true">

For a good read on WFC method see [this
article](https://www.gridbugs.org/wave-function-collapse/). It's particularly
useful for procedural generation of game levels.

All feedback is very welcome. Thanks.

## HOW TO USE THE LIBRARY

One file in your project should include `wfc.h` like this:

```c
        #define WFC_IMPLEMENTATION
        #include "wfc.h"
```

Other files can also include and use `wfc.h` but they shouldn't define
`WFC_IMPLEMENTATION` macro.

Usage:

```c
        struct wfc *wfc = wfc_overlapping(
            128,             // Output image width in pixels
            128,             // Output image height in pixels
            input_image,     // Input image that will be cut into tiles
            3,               // Tile width in pixels
            3,               // Tile height in pixels
            1,               // Expand input image on the right and bottom
            1,               // Add horizontal flips of all tiles
            1,               // Add vertical flips of all tiles
            1                // Add n*90deg rotations of all tiles
        );

        wfc_run(wfc, -1);    // Run Wave Function Collapse
                             // -1 means no limit on iterations
        struct wfc_image *output_image = wfc_output_image(wfc);
        wfc_destroy(wfc);
        // use output_image->data
        // wfc_img_destroy(output_image);
```

By default you work with `struct wfc_image` for inputs and outputs.

```c
        struct wfc_image {
            unsigned char *data;
            int component_cnt;
            int width;
            int height;
         }
```

`data` is tightly packed without padding. Each pixel consists of
`component_cnt` components (e.g., four components for rgba format).
The output image will have the same number of components as the input
image.

`wfc_run` returns 0 if it cannot find a solution. You can try again like so:

```c
        wfc_init(wfc);
        wfc_run(wfc, -1);
```

### Working with image files

khu_wfc can optionally use [stb_image.h](https://github.com/nothings/stb) and [stb_write.h](https://github.com/nothings/stb) to provide
convenience functions for working directly with image files.

You will normally place `stb_image.h` and `stb_write.h` in the same
directory as `wfc.h` and include their implementations in one of the
project files:

```c
        #define STB_IMAGE_IMPLEMENTATION
        #define STB_IMAGE_WRITE_IMPLEMENTATION
        #include "stb_image.h"
        #include "stb_image_write.h"
```

Further, you will instruct `wfc.h` to use stb:

```c
        #define WFC_IMPLEMENTATION
        #define WFC_USE_STB
        #include "wfc.h"
```

Usage:

```c
        struct wfc_image *input_image = wfc_img_load("input.png");
        struct wfc *wfc = wfc_overlapping(
            ...
            input_image,
            ...
        );

        wfc_run(wfc, -1);    // Run Wave Function Collapse
                             // -1 means no restriction on number of iterations
        wfc_export(wfc, "output.png");
        wfc_img_destroy(input_image);
        wfc_destroy(wfc);
```

Extra functions enabled by the inclusion of stb:

```c
        struct wfc_image *image = wfc_img_load("image.png")
        wfc_img_save(image, "image.png")
        wfc_export(wfc, "output.png")
        wfc_export_tiles(wfc, "directory")
        // don't forget to wfc_img_destroy(image) loaded images
```

## COMMAND-LINE TOOL

The command-line tool is based on khu_wfc and can be used to generate WFC
images. The tool depends on [stb_image.h](https://github.com/nothings/stb) and [stb_write.h](https://github.com/nothings/stb).
Place both files in the same directory as `wfctool.c`.

```
        make
        ./wfc
```

Run `./wfc` to see available options


Basic usage:

```
        ./wfc -m overlapping -w 128 -h 128 input.png output.png
```

## THANKS

Thanks for using khu_wfc. If you find any bugs, have questions, or miss
a feature please let me know. Also, if you'd like to share your works
it's very appreciated.

samp.krystian at gmail.com

