# wfc

Single-file Wave Function Collapse library in C, plus a command-line tool

- License: MIT
- Version: 0.7

This is an early version that supports the overlapping WFC method.
The method takes a small input image and generates a larger output image which is
locally similar to the input image. Here're a few examples of
input/output pairs:

<img width="950px" src="https://user-images.githubusercontent.com/947457/150605257-ba54c0fe-734d-4458-89d6-7fd54ea99495.png">

The WFC is often used for procedural map generation, but is not limited to this use-case.

The library is very performant and includes a number of optimizations not found in other implementations. As an example, the generation of the above 128x128 images on a MacBook Air M1 (2020) took: 1.35, 0.92, 0.31, 7.7, 1.74, and 0.67 seconds respectively. This includes the image loading/saving time. The tiles were 3x3 and included all flips and rotations.

## LANGUAGE BINDINGS

- [Rust bindings](https://crates.io/crates/wfc-rs) -- by [Ryan Noah](https://github.com/nsmryan)

## HOW TO USE THE LIBRARY

One file in your project should include `wfc.h` like this:

```c
        #define WFC_IMPLEMENTATION
        #include "wfc.h"
```

Other files can also include and use `wfc.h` but they shouldn't define
`WFC_IMPLEMENTATION` macro.

Alternatively, you can build a traditional .o file with `make wfc.o` and use
`wfc.h` as a regular header file.

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

wfc can optionally use [stb_image.h](https://github.com/nothings/stb) and [stb_image_write.h](https://github.com/nothings/stb) to provide
convenience functions for working directly with image files.

You will normally place `stb_image.h` and `stb_image_write.h` in the same
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

The command-line tool uses the library and allows to generate WFC images.
The tool depends on [stb_image.h](https://github.com/nothings/stb)
and [stb_image_write.h](https://github.com/nothings/stb). Place both files in the same
directory as `wfctool.c`.

```
        make
        ./wfc
```

Run `./wfc` to see available options


Basic usage:

```
        ./wfc -m overlapping samples/wrinkles.png output.png
```

## THANKS

Thanks for using wfc. If you find any bugs, have questions, or feedback please
let me know. Also, if you'd like to share your works it's very appreciated.

samp.krystian at gmail.com

