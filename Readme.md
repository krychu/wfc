# Wave Function Collapse

Single-file Wave Function Collapse library / command-line tool in C.

v.0.1 (pre-alpha)

The library / command-line tool currently require raylib.

## Use as a library

One file in your project should include wfc.c like this:

```c
  #define WFC_IMPLEMENTATION
  #include "wfc.c"
```

Other files in the project can also include `wfc.c` and use its functionality but
they shouldn't define `WFC_IMPLEMENTATION` macro.

**Usage example:**

```c
  struct wfc *wfc = wfc_create(
    128,                     // Output image width in pixels
    128,                     // Output image height in pixels
    "input_image.png",       // Input image
    3,                       // Tile width in pixels
    3,                       // Tile height in pixels
    1,                       // Wrap input image on the right and bottom
    1,                       // Add horizontal flips of all tiles
    1,                       // Add vertical flips of all tiles
    1                        // Add n*90deg rotations of all tiles
  );
  
  wfc_run(wfc, -1);          // Run Wave Function Collapse
                             // -1 means no restriction on number of iterations
  wfc_export(wfc, "output_image.png");
  wfc_destroy(wfc);
```

## Use as a command-line tool

```
make
./wfc
```

Run `./wfc` to see available options.

At the moment the build has been configured / tested on a Mac.

# Gallery
