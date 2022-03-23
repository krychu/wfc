// wfc
//
// Single-file Wave Function Collapse library
//
// Author:  Krystian Samp (samp.krystian at gmail.com)
// License: MIT
// Version: 0.7
//
// This is an early version that supports the overlapping WFC method.
// All feedback is very welcome and best sent by email. Thanks.
//
//
// HOW TO USE
// =============================================================================
//
// One file in your project should include wfc.h like this:
//
//         #define WFC_IMPLEMENTATION
//         #include "wfc.h"
//
// Other files can also include and use wfc.h but they shouldn't define
// WFC_IMPLEMENTATION macro.
//
// Usage:
//
//         struct wfc *wfc = wfc_overlapping(
//             128,             // Output image width in pixels
//             128,             // Output image height in pixels
//             input_image,     // Input image that will be cut into tiles
//             3,               // Tile width in pixels
//             3,               // Tile height in pixels
//             1,               // Expand input image on the right and bottom
//             1,               // Add horizontal flips of all tiles
//             1,               // Add vertical flips of all tiles
//             1                // Add n*90deg rotations of all tiles
//         );
//
//         wfc_run(wfc, -1);    // Run Wave Function Collapse
//                              // -1 means no limit on iterations
//         struct wfc_image *output_image = wfc_output_image(wfc);
//         wfc_destroy(wfc);
//         // use output_image->data
//         // wfc_img_destroy(output_image);
//
// By default you work with struct wfc_image for inputs and outputs:
//
// struct wfc_image {
//         unsigned char *data;
//         int component_cnt;
//         int width;
//         int height;
// }
//
// Data is tightly packed without padding. Each pixel consists of
// component_cnt components (e.g., four components for rgba format).
// The output image will have the same number of components as the input
// image.
//
// wfc_run returns 0 if it cannot find a solution. You can try again like so:
//
//         wfc_init(wfc);
//         wfc_run(wfc, -1);
//
//
// Working with image files
// ----------------------------------------
//
// wfc can optionally use stb_image.h and stb_write.h to provide
// convenience functions for working directly with image files.
//
// You will normally place stb_image.h and stb_write.h in the same
// directory as wfc.h and include their implementations in one of the
// project files:
//
//         #define STB_IMAGE_IMPLEMENTATION
//         #define STB_IMAGE_WRITE_IMPLEMENTATION
//         #include "stb_image.h"
//         #include "stb_image_write.h"
//
// Further, you will instruct wfc.h to use stb:
//
//         #define WFC_IMPLEMENTATION
//         #define WFC_USE_STB
//         #include "wfc.h"
//
// Usage:
//
//         struct wfc_image *input_image = wfc_img_load("input.png");
//         struct wfc *wfc = wfc_overlapping(
//             ...
//             input_image,
//             ...
//         );
//
//         wfc_run(wfc, -1);    // Run Wave Function Collapse
//                              // -1 means no restriction on number of iterations
//         wfc_export(wfc, "output.png");
//         wfc_img_destroy(input_image);
//         wfc_destroy(wfc);
//
//
// Extra functions enabled by the inclusion of stb:
//
//         struct wfc_image *image = wfc_img_load("image.png")
//         wfc_img_save(image, "image.png")
//         wfc_export(wfc, "output.png")
//         wfc_export_tiles(wfc, "directory")
//         // don't forget to wfc_img_destroy(image) loaded images
//
//
// THANKS
// =============================================================================
//
// Thanks for using wfc. If you find any bugs, have questions, or miss
// a feature please let me know. Also, if you'd like to share your works
// it's very appreciated. Please use my email at the top of the file.
//

#ifndef WFC_H
#define WFC_H

#ifdef __cplusplus
extern "C" {
#endif

struct wfc;

struct wfc_image {
  unsigned char *data;
  int component_cnt;
  int width;
  int height;
};

struct wfc *wfc_overlapping(int output_width,              // Output width in pixels
                            int output_height,             // Output height in pixels
                            struct wfc_image *image,       // Input image to be cut into tiles
                            int tile_width,                // Tile width in pixels
                            int tile_height,               // Tile height in pixels
                            int expand_input,              // Wrap input image on right and bottom
                            int xflip_tiles,               // Add xflips of all tiles
                            int yflip_tiles,               // Add yflips of all tiles
                            int rotate_tiles);             // Add n*90deg rotations of all tiles

void wfc_init(struct wfc *wfc); // Resets wfc generation, wfc_run can be called again
int wfc_run(struct wfc *wfc, int max_collapse_cnt);
int wfc_export(struct wfc *wfc, const char *filename);
void wfc_destroy(struct wfc *wfc);

#ifdef __cplusplus
}
#endif

#endif // WFC_H

#if defined(WFC_USE_STB)
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#endif

#ifdef WFC_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#define WFC_MAX_PROP_CNT 1000

#ifndef WFC_USE_STB

#define wfc_img_save(...) wfc__nofunc_int("wfc_img_save", "requires stb", __VA_ARGS__)
#define wfc_img_load(...) wfc__nofunc_ptr("wfc_img_load", "requires stb", __VA_ARGS__)
#define wfc_export(...)   wfc__nofunc_int("wfc_export", "requires_stb", __VA_ARGS__)
#define wfc_export_tiles(...) wfc__nofunc_int("wfc_export_tiles", "requires_stb", __VA_ARGS__)

#endif

#if defined(WFC_DEBUG) || defined(WFC_TOOL)

#define print_progress(int_progress) \
  do { \
    printf("\rcells collapsed:      %d", int_progress); \
    fflush(stdout); \
  } while(0)

#define print_endprogress() printf("\n")
#define wfcassert(test) assert(test)
#define p(...) printf(__VA_ARGS__)

#else

#define print_progress(int_progress)
#define print_endprogress()
#define wfcassert(test)
#define p(...)

#endif

enum wfc__direction {WFC_UP,WFC_DOWN,WFC_LEFT,WFC_RIGHT};
int directions[4] = {WFC_UP, WFC_DOWN, WFC_LEFT, WFC_RIGHT};
enum wfc__method {WFC_METHOD_OVERLAPPING, WFC_METHOD_TILED};

// Rules are stored in tiles
struct wfc__tile {
  struct wfc_image *image;

  int freq;                    // Relative frequency of the tile. Typically a
                               // count of tile occurrences in the input image.
                               // It affects the probability of the tile being
                               // selected when collapsing a cell.
};

struct wfc__cell {
  int *tiles;                  // Possible tiles in the cell (initially all)
  int tile_cnt;

  int sum_freqs;               // Sum of tile frequencies used to calculate
                               // entropy and randomly pick a tile when
                               // collapsing a tile.

  double entropy;              // Shannon entropy. Cell with the smallest entropy
                               // is picked to be collapsed next.
};

struct wfc__prop {
  int src_cell_idx;
  int dst_cell_idx;
  enum wfc__direction direction;
};

// One structure for overlapping and tiled models
struct wfc {
  enum wfc__method method;     // overlapping or tiled?
  unsigned int seed;

  /* tiles */

  struct wfc_image *image;     // Input image, not always required
  int tile_width;              // Tile width in pixels
  int tile_height;             // Tile height in pixels
  int expand_input;
  int xflip_tiles;
  int yflip_tiles;
  int rotate_tiles;
  struct wfc__tile *tiles;     // All available tiles
  int tile_cnt;
  int sum_freqs;

  /* output */

  int output_width;            // Output width in pixels
  int output_height;           // Output height in pixels
  struct wfc__cell *cells;     // One per output pixel
  int cell_cnt;                // width * height

  /* in-use */

  struct wfc__prop *props;     // Propagation updates
  int prop_cnt;
  int prop_idx;                // Current index into props, used to scan
                               // props forward to check whether a
                               // a candidate prop is already there
  int collapsed_cell_cnt;

  // These are the rules. A matrix lookup of allowed tile pairs in each
  // direction. allowed_tiles[d][src_idx*tile_cnt + dst_idx] is 1 or 0
  // depending on whether dst_idx tile can be placed next to the src_idx
  // tile in the direction d.
  //
  // In the overlapping method tiles are allowed next to each other if
  // their content overlaps, excluding the edges.
  int *allowed_tiles[4];
};

#ifdef WFC_DEBUG
static const char *wfc__direction_strings[4] = {"up","down","left","right"};

static void wfc__print_prop(struct wfc__prop *p, const char *prefix)
{
  printf("%s%d -> %s -> %d\n", prefix, p->src_cell_idx, direction_strings[p->direction], p->dst_cell_idx);
}

static void wfc__print_props(struct wfc__prop *p, int prop_cnt, const char *prefix)
{
  for (int i=0; i<prop_cnt; i++) {
    print_prop(&(p[i]), prefix);
  }
}
#endif // WFC_DEBUG

////////////////////////////////////////////////////////////////////////////////
//
// Img helpers
//
////////////////////////////////////////////////////////////////////////////////

#ifdef WFC_USE_STB

// Return 0 on failure, non-0 on success
int wfc_img_save(struct wfc_image *image, const char *filename)
{
  int len = strlen(filename);
  char extension[5];

  if (len < 4)
    goto UNKNOWNFORMAT;

  strcpy(extension, filename + len - 4);
  for (int i=0; i<4; i++)
    extension[i] = tolower(extension[i]);

  if (strcmp(extension, ".png")==0)
    return stbi_write_png(filename, image->width, image->height, image->component_cnt, image->data, image->width * image->component_cnt);
  else if (strcmp(extension, ".bmp")==0)
    return stbi_write_bmp(filename, image->width, image->height, image->component_cnt, image->data);
  else if (strcmp(extension, ".tga")==0)
    return stbi_write_tga(filename, image->width, image->height, image->component_cnt, image->data);
  else if (strcmp(extension, ".jpg")==0)
    return stbi_write_jpg(filename, image->width, image->height, image->component_cnt, image->data, 100);

 UNKNOWNFORMAT:
  printf("error: wfc_imgsave - unknown format (%s)\n", filename);
  return 0;
}

// Return NULL on error
struct wfc_image *wfc_img_load(const char *filename)
{
  struct wfc_image *image = malloc(sizeof(*image));
  if (image == NULL) {
    p("wfc_img_load: error\n");
    return NULL;
  }

  image->data = stbi_load(filename, &image->width, &image->height, &image->component_cnt, 0);

  if (image->data == NULL) {
    p("wfc_img_load: error\n");
    free(image);
    return NULL;
  }

  return image;
}

#endif // WFC_USE_STB

// Return NULL on error
struct wfc_image *wfc_img_copy(struct wfc_image *image)
{
  struct wfc_image *copy = malloc(sizeof(*copy));
  if (copy == NULL) {
    p("wfc_img_copy: error\n");
    return NULL;
  }

  copy->width = image->width;
  copy->height = image->height;
  copy->component_cnt = image->component_cnt;
  copy->data = malloc(sizeof(*copy->data) * copy->width*copy->height*copy->component_cnt);
  if (copy->data == NULL) {
    p("wfc_img_copy: error\n");
    free(copy);
    return NULL;
  }

  memcpy(copy->data, image->data, image->width*image->height*image->component_cnt);
  return copy;
}

// Return NULL on error
struct wfc_image *wfc_img_create(int width, int height, int component_cnt)
{
  struct wfc_image *image = malloc(sizeof(*image));
  if (image == NULL) {
    p("wfc_img_create: error\n");
    return NULL;
  }

  image->width = width;
  image->height = height;
  image->component_cnt = component_cnt;
  image->data = malloc(sizeof(*image->data) * width * height * component_cnt);
  if (image->data == NULL) {
    p("wfc_img_create: error\n");
    free(image->data);
    return NULL;
  }

  return image;
}

void wfc_img_destroy(struct wfc_image *image)
{
  free(image != NULL ? image->data : NULL);
  free(image);
}

// Return NULL on error
static struct wfc_image *wfc__img_expand(struct wfc_image *image, int xexp, int yexp)
{
  struct wfc_image *exp_image = wfc_img_create(image->width + xexp, image->height + yexp, image->component_cnt);
  if (exp_image == NULL) {
    p("wfc__img_expand: error\n");
    return NULL;
  }

  for (int y=0; y<exp_image->height; y++) {
    memcpy(&exp_image->data[y * exp_image->width * image->component_cnt],
           &image->data[(y % image->height) * image->width * image->component_cnt],
           image->width * image->component_cnt);
    memcpy(&exp_image->data[y * exp_image->width * image->component_cnt + image->width * image->component_cnt],
           &image->data[(y % image->height) * image->width * image->component_cnt],
           xexp * image->component_cnt);
  }

  return exp_image;
}

// Return 1 if the two images overlap perfectly except the edges in the given direction, 0 otherwise.
static int wfc__img_cmpoverlap(struct wfc_image *a, struct wfc_image *b, enum wfc__direction direction)
{
  int a_offx, a_offy, b_offx, b_offy, width, height;

  switch (direction) {
  case WFC_UP:
    a_offx = 0; a_offy = 0;
    b_offx = 0; b_offy = 1;
    width = a->width;
    height = a->height-1;
    break;
  case WFC_DOWN:
    a_offx = 0; a_offy = 1;
    b_offx = 0; b_offy = 0;
    width = a->width;
    height = a->height-1;
    break;
  case WFC_LEFT:
    a_offx = 0; a_offy = 0;
    b_offx = 1; b_offy = 0;
    width = a->width-1;
    height = a->height;
    break;
  case WFC_RIGHT:
    a_offx = 1; a_offy = 0;
    b_offx = 0; b_offy = 0;
    width = a->width-1;
    height = a->height;
    break;
  default:
    printf("wfc error: wrong direction (%d)\n", direction);
    return 0;
  };

  for (int y=0; y<height; y++) {
    int a_y = a_offy + y;
    int b_y = b_offy + y;

    if (memcmp(&(a->data[(y+a_offy)*a->width*a->component_cnt + a_offx*a->component_cnt]),
                &(b->data[(y+b_offy)*a->width*a->component_cnt + b_offx*a->component_cnt]),
                width*a->component_cnt)) {
      return 0;
    }
  }
  return 1;
}

// Return NULL on error
static struct wfc_image *wfc__img_flip_horizontally(struct wfc_image *image)
{
  struct wfc_image *copy = wfc_img_copy(image);
  if (copy == NULL) {
    p("wfc__img_flip_horizontally: error\n");
    return NULL;
  }

  for (int y=0; y<image->height; y++) {
    for (int x=0; x<image->width/2; x++) {
      unsigned char *src = &( image->data[y*image->width*image->component_cnt + x*image->component_cnt] );
      unsigned char *dst = &( copy->data[y*image->width*image->component_cnt + (image->width-1-x)*image->component_cnt] );
      memcpy(dst, src, image->component_cnt);

      src = &( image->data[y*image->width*image->component_cnt + (image->width-1-x)*image->component_cnt] );
      dst = &( copy->data[y*image->width*image->component_cnt + x*image->component_cnt] );
      memcpy(dst, src, image->component_cnt);
    }
  }
  return copy;
}

// Return NULL on error
static struct wfc_image *wfc__img_flip_vertically(struct wfc_image *image)
{
  struct wfc_image *copy = wfc_img_copy(image);
  if (copy == NULL) {
    p("wfc__img_flip_vertically: error\n");
    return NULL;
  }

  for (int y=0; y<image->height/2; y++) {
    unsigned char *src = &( image->data[y*image->width*image->component_cnt] );
    unsigned char *dst = &( copy->data[(image->height-1-y)*image->width*image->component_cnt] );
    memcpy(dst, src, image->width*image->component_cnt);

    src = &( image->data[(image->height-1-y)*image->width*image->component_cnt] );
    dst = &( copy->data[y*image->width*image->component_cnt] );
    memcpy(dst, src, image->width*image->component_cnt);
  }
  return copy;
}

static struct wfc_image *wfc__img_rotate90(struct wfc_image *image, int n) {
  wfcassert(n>0);

  n %= 4;

  struct wfc_image *copy;
  if (n%2) {
    copy = wfc_img_create(image->height, image->width, image->component_cnt);
  } else {
    copy = wfc_img_create(image->width, image->height, image->component_cnt);
  }
  if (copy == NULL) {
    p("wfc__img_rotate90: error\n");
    return NULL;
  }

  for (int y=0; y<image->height; y++) {
    for (int x=0; x<image->width; x++) {
      unsigned char components[4];
      memcpy(components, &(image->data[y * image->width * image->component_cnt + x * image->component_cnt]), image->component_cnt);
      if (n==1) {
        memcpy(&(copy->data[x * copy->width * copy->component_cnt + (copy->width - y - 1) * copy->component_cnt]),
               components,
               image->component_cnt);
      } else if (n==2) {
        memcpy(&(copy->data[(copy->height - y - 1) * copy->width * copy->component_cnt + (copy->width - x - 1) * copy->component_cnt]),
               components,
               image->component_cnt);
      } else if (n==3) {
        memcpy(&(copy->data[(copy->height - x - 1) * copy->width * copy->component_cnt + y * copy->component_cnt]),
               components,
               image->component_cnt);
      } else {
        printf("error: wfc__img_rotate90, n=%d\n", n);
        exit(1);
      }
    }
  }
  return copy;
}

// Return 1 if the two images are the same, 0 otherwise.
static int wfc__img_cmp(struct wfc_image *a, struct wfc_image *b)
{
  if (a->width!=b->width || a->height!=b->height || a->component_cnt!=b->component_cnt) {
    return 0;
  }
  return memcmp(a->data, b->data, a->width*a->height*a->component_cnt) == 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// WFC: Utils (Method-independent)
//
////////////////////////////////////////////////////////////////////////////////

static int wfc__nofunc_int(const char *func_name, const char *msg, ...)
{
  printf("%s: %s\n", func_name, msg);
  return -1;
}

static void *wfc__nofunc_ptr(const char *func_name, const char *msg, ...)
{
  printf("%s: %s\n", func_name, msg);
  return NULL;
}

static void wfc_destroy_cells(int *cells)
{
  free(cells);
}

// Return NULL on error
static int *wfc_cells(struct wfc *wfc)
{
  int *cells = malloc(sizeof(*cells) * wfc->cell_cnt);
  if (cells == NULL)
    return NULL;

  for (int i=0; i<wfc->cell_cnt; i++)
    cells[i] = wfc->cells[i].tiles[0];

  return cells;
}

struct wfc_image *wfc_output_image(struct wfc *wfc)
{
  struct wfc_image *image = wfc_img_create(wfc->output_width, wfc->output_height, wfc->image->component_cnt);
  if (image == NULL) {
    p("wfc_export: error\n");
    return 0;
  }

  for (int y=0; y<wfc->output_height; y++) {
    for (int x=0; x<wfc->output_width; x++) {
      struct wfc__cell *cell = &( wfc->cells[y * wfc->output_width + x] );

      double components[4] = {0, 0, 0, 0};
      for (int i=0; i<cell->tile_cnt; i++) {
        struct wfc__tile *tile = &( wfc->tiles[ cell->tiles[i] ] );
        for (int j=0; j<wfc->image->component_cnt; j++) {
          components[j] += tile->image->data[j];
        }
      }

      for (int i=0; i<wfc->image->component_cnt; i++) {
        image->data[y * wfc->output_width * wfc->image->component_cnt + x * wfc->image->component_cnt + i] = (unsigned char)(components[i] / cell->tile_cnt);
      }
    }
  }

  return image;
}

#ifdef WFC_USE_STB

// Return 0 on error, non-0 on success
// dep: stb
int wfc_export(struct wfc *wfc, const char *filename)
{
  struct wfc_image *image = wfc_output_image(wfc);
  int rv = wfc_img_save(image, filename);
  wfc_img_destroy(image);

  return rv;
}

// Return 0 on error, non-0 on success
// dep: stb
int wfc_export_tiles(struct wfc *wfc, const char *path)
{
  char filename[128];
  for (int i=0; i<wfc->tile_cnt; i++) {
    sprintf(filename, "%s/%d.png", path, i);
    struct wfc__tile *tile = &wfc->tiles[i];
    if (wfc_img_save(tile->image, filename) == 0) {
      p("wfc_export_tiles: error\n");
      return 0;
    }
  }
  return 1;
}

#endif // WFC_USE_STB

// Return NULL on error
// Assumes the tile fits in the image
static struct wfc_image *wfc__create_tile_image(struct wfc_image *image, int x, int y, int tile_width, int tile_height)
{
  struct wfc_image *tile_image = wfc_img_create(tile_width, tile_height, image->component_cnt);
  if (tile_image == NULL) {
    p("wfc__create_tile_image: error\n");
    return NULL;
  }

  tile_image->width = tile_width;
  tile_image->height = tile_height;
  tile_image->component_cnt = image->component_cnt;

  for (int i=0; i<tile_height; i++) {
    memcpy(&tile_image->data[i * tile_width * image->component_cnt],
           &image->data[(y+i) * image->width * image->component_cnt + x * image->component_cnt],
           tile_width * image->component_cnt);
  }

  return tile_image;
}

// Return 0 on error
static int wfc__add_overlapping_images(struct wfc__tile *tiles, struct wfc_image *image, int xcnt, int ycnt, int tile_width, int tile_height)
{
  int tile_cnt = xcnt * ycnt;
  for (int y=0; y<ycnt; y++)
    for (int x=0; x<xcnt; x++) {
      struct wfc__tile *tile = &( tiles[y*xcnt + x] );
      tile->freq = 1;
      tile->image = wfc__create_tile_image(image, x, y, tile_width, tile_height);
      if (tile->image == NULL)
        goto CLEANUP;
    }

  return 1;

 CLEANUP:
  for (int i=0; i<tile_cnt; i++)
    wfc_img_destroy(tiles[i].image);

  return 0;
}

// Return 0 on error, non-0 on success
// flip_direction: 0 - horizontal, 1 vertical
static int wfc__add_flipped_images(struct wfc__tile *tiles, int tile_idx, int flip_direction)
{
  for (int i=0; i<tile_idx; i++) {
    struct wfc__tile *src = &tiles[i];
    struct wfc__tile *dst = &tiles[tile_idx + i];
    dst->freq = 1;
    if (flip_direction == 0)
      dst->image = wfc__img_flip_horizontally(src->image);
    else
      dst->image = wfc__img_flip_vertically(src->image);

    if (dst->image == NULL)
      goto CLEANUP;
  }

  return 1;

 CLEANUP:
  p("wfc__add_flipped_tiles: error\n");
  for (int i=0; i<tile_idx; i++)
    wfc_img_destroy(tiles[tile_idx + i].image);

  return 0;
}

// Return 0 on error, non-0 on success
static int wfc__add_rotated_images(struct wfc__tile *tiles, int tile_idx)
{
  for (int i=0; i<tile_idx; i++) {
    for (int j=0; j<3; j++) {
      struct wfc__tile *src = &tiles[i];
      struct wfc__tile *dst = &tiles[tile_idx + i*3 + j];
      dst->freq = 1;
      dst->image = wfc__img_rotate90(src->image, j+1);

      if (dst->image == NULL)
        goto CLEANUP;
    }
  }

  return 1;

 CLEANUP:
  p("wfc__add_rotated_tiles: error\n");
  for (int i=tile_idx; i<tile_idx*4; i++)
    wfc_img_destroy(tiles[i].image);

  return 0;
}

// Return unique tiles with frequencies
// Return 0 on error, non-0 on success
static int wfc__remove_duplicate_tiles(struct wfc__tile **tiles, int *tile_cnt)
{
  int unique_cnt = 1;
  for (int j=1; j<*tile_cnt; j++) {
    int unique = 1;
    for (int k=0; k<unique_cnt; k++) {
      if (wfc__img_cmp((*tiles)[j].image, (*tiles)[k].image)) {
        unique = 0;
        (*tiles)[k].freq++;
        break;
      }
    }

    if (unique) {
      if (unique_cnt != j) {
        struct wfc__tile tmp = (*tiles)[unique_cnt];
        (*tiles)[unique_cnt] = (*tiles)[j];
        (*tiles)[j] = tmp;
      }
      unique_cnt++;
    }
  }

  for (int i=unique_cnt; i<*tile_cnt; i++)
    wfc_img_destroy((*tiles)[i].image);

  struct wfc__tile *unique_tiles = realloc(*tiles, sizeof(**tiles) * unique_cnt);
  if (unique_tiles == NULL) {
    p("wfc__remove_duplicate_tiles: error\n");
    return 0;
  }

  *tiles = unique_tiles;
  *tile_cnt = unique_cnt;

  return 1;
}

////////////////////////////////////////////////////////////////////////////////
//
// WFC: Solve (Method-independent)
//
////////////////////////////////////////////////////////////////////////////////

static void wfc__destroy_props(struct wfc__prop *props)
{
  free(props);
}

static struct wfc__prop *wfc__create_props(int cell_cnt)
{
  struct wfc__prop *props = malloc(sizeof(*props) * cell_cnt * WFC_MAX_PROP_CNT);
  return props;
}

static void wfc__destroy_cells(struct wfc__cell *cells, int cell_cnt)
{
  free(cells[0].tiles);
  free(cells);
}

// Return NULL on error
static struct wfc__cell *wfc__create_cells(int cell_cnt, int tile_cnt)
{
  struct wfc__cell *cells = malloc(sizeof(*cells) * cell_cnt);
  if (cells == NULL)
    goto CLEANUP;

  for (int i=0; i<cell_cnt; i++)
    cells[i].tiles = NULL;

  cells[0].tiles = malloc(sizeof(*(cells[0].tiles)) * tile_cnt * cell_cnt);
  for (int i=1; i<cell_cnt; i++) {
    cells[i].tiles = cells[0].tiles + i * tile_cnt;
    if (cells[i].tiles == NULL)
      goto CLEANUP;
  }

  return cells;

 CLEANUP:
  p("wfc__create_cells: error\n");
  wfc__destroy_cells(cells, cell_cnt);
  return NULL;
}

static void wfc__destroy_tiles(struct wfc__tile *tiles, int tile_cnt)
{
  if (tiles == NULL)
    return;

  for (int i=0; i<tile_cnt; i++) {
    struct wfc__tile *t = &tiles[i];
    wfc_img_destroy(t->image);
  }

  free(tiles);
}

// Return 0 on error
static struct wfc__tile *wfc__create_tiles(int tile_cnt)
{
  struct wfc__tile *tiles = malloc(sizeof(*tiles) * tile_cnt);
  if (tiles == NULL)
    goto CLEANUP;

  for (int i=0; i<tile_cnt; i++) {
    tiles[i].image = NULL;
    tiles[i].freq = 0;
  }

  return tiles;

 CLEANUP:
  p("wfc__create_tiles: error\n");
  wfc__destroy_tiles(tiles, tile_cnt);

  return NULL;
}

static void wfc__destroy_allowed_tiles(int *allowed_tiles[4])
{
  free(allowed_tiles[0]);
}

// Return 0 on error
static int wfc__create_allowed_tiles(int *allowed_tiles[4], int tile_cnt)
{
  allowed_tiles[0] = malloc(sizeof(*allowed_tiles[0]) * tile_cnt * tile_cnt * 4);
  for (int i=1; i<4; i++)
    allowed_tiles[i] = allowed_tiles[0] + i * tile_cnt * tile_cnt;

  return 1;

 CLEANUP:
  p("wfc__create_allowed_tiles: error\n");
  wfc__destroy_allowed_tiles(allowed_tiles);

  return 0;
}

static void wfc__add_prop(struct wfc *wfc, int src_cell_idx, int dst_cell_idx, enum wfc__direction direction)
{
  // TODO: check for wfc->prop_cnt == WFC_MAX_PROP_CNT

  struct wfc__prop *p = &( wfc->props[wfc->prop_cnt] );
  (wfc->prop_cnt)++;
  p->src_cell_idx = src_cell_idx;
  p->dst_cell_idx = dst_cell_idx;
  p->direction = direction;
}

// add prop to update cell above the cell_idx
static void wfc__add_prop_up(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx - wfc->output_width >= 0) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx - wfc->output_width, WFC_UP);
  }
}

static void wfc__add_prop_down(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx + wfc->output_width < wfc->cell_cnt) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx + wfc->output_width, WFC_DOWN);
  }
}

static void wfc__add_prop_left(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx % wfc->output_width != 0) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx - 1, WFC_LEFT);
  }
}

static void wfc__add_prop_right(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx % wfc->output_width != wfc->output_width - 1) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx + 1, WFC_RIGHT);
  }
}

// Does the cell enable tile in the direction?
static int wfc__tile_enabled(struct wfc *wfc, int tile_idx, int cell_idx, enum wfc__direction d)
{
  struct wfc__cell *cell = &( wfc->cells[cell_idx] );
  int *tiles = cell->tiles;

  // Tile is enabled if any of the cell's tiles allowes it in
  // the specified diretion
  int tile_cnt = wfc->tile_cnt;
  for (int i=0, cnt=cell->tile_cnt; i<cnt; i++) {
    if (wfc->allowed_tiles[d][tiles[i] * tile_cnt + tile_idx]) {
      return 1;
    }
  }

  return 0;
}

// Checks whether particular prop is already added and pending, in which
// case there is no point of adding the same prop again.
//
// 1 - prop is added, 0 - prop is not added
static int wfc__is_prop_pending(struct wfc *wfc, int cell_idx, enum wfc__direction d) {
  for (int i=wfc->prop_idx+1; i<wfc->prop_cnt; i++) {
    struct wfc__prop *p = &( wfc->props[i] );
    if (p->src_cell_idx == cell_idx && p->direction == d) {
      return 1;
    }
  }
  return 0;
}

// Updates tiles in the destination cell to those that are allowed by the source cell
// and propagate updates
//
// Return 0 on error
static int wfc__propagate_prop(struct wfc *wfc, struct wfc__prop *p)
{
  int new_cnt = 0;

  struct wfc__cell *dst_cell = &( wfc->cells[ p->dst_cell_idx ] );

  // Go through all destination tiles and check whether they are enabled by the source cell
  for (int i=0, cnt=dst_cell->tile_cnt; i<cnt; i++) {
    int possible_dst_tile_idx = dst_cell->tiles[i];

    // If a destination tile is enabled by the source cell, keep it
    if (wfc__tile_enabled(wfc, possible_dst_tile_idx, p->src_cell_idx, p->direction)) {
      dst_cell->tiles[new_cnt] = possible_dst_tile_idx;
      new_cnt++;
    } else {
      int freq = wfc->tiles[possible_dst_tile_idx].freq;
      double p = ((double)freq) / wfc->sum_freqs;
      dst_cell->entropy += p*log(p);
      dst_cell->sum_freqs -= freq;
      if (dst_cell->sum_freqs == 0) // no options left TODO: remove
        return 0;
    }
  }

  if (!new_cnt) {
    return 0;
  }

  if (dst_cell->tile_cnt != new_cnt) {
    if (new_cnt == 1) wfc->collapsed_cell_cnt++;
    if (p->direction != WFC_DOWN && !wfc__is_prop_pending(wfc, p->dst_cell_idx, WFC_UP)) wfc__add_prop_up(wfc, p->dst_cell_idx);
    if (p->direction != WFC_UP && !wfc__is_prop_pending(wfc, p->dst_cell_idx, WFC_DOWN)) wfc__add_prop_down(wfc, p->dst_cell_idx);
    if (p->direction != WFC_RIGHT && !wfc__is_prop_pending(wfc, p->dst_cell_idx, WFC_LEFT)) wfc__add_prop_left(wfc, p->dst_cell_idx);
    if (p->direction != WFC_LEFT && !wfc__is_prop_pending(wfc, p->dst_cell_idx, WFC_RIGHT)) wfc__add_prop_right(wfc, p->dst_cell_idx);
  }

  dst_cell->tile_cnt = new_cnt;

  return 1;
}

// Return 0 on error (contradiction)
static int wfc__propagate(struct wfc *wfc, int cell_idx)
{
  wfc->prop_cnt = 0;

  wfc__add_prop_up(wfc, cell_idx);
  wfc__add_prop_down(wfc, cell_idx);
  wfc__add_prop_left(wfc, cell_idx);
  wfc__add_prop_right(wfc, cell_idx);

  for (int i=0; i<wfc->prop_cnt; i++) {
    wfc->prop_idx = i;
    struct wfc__prop *p = &( wfc->props[i] );
    if (!wfc__propagate_prop(wfc, p)) {
      return 0;
    }
  }

  return 1;
}

// Return 0 on error (contradiction)
static int wfc__collapse(struct wfc *wfc, int cell_idx)
{
  int remaining = rand() % wfc->cells[cell_idx].sum_freqs;
  for (int i=0; i<wfc->cells[cell_idx].tile_cnt; i++) {
    int freq = wfc->tiles[ wfc->cells[cell_idx].tiles[i] ].freq;
    if (remaining >= freq) {
      remaining -= freq;
    } else {
      wfc->cells[cell_idx].tiles[0] = wfc->cells[cell_idx].tiles[i];
      wfc->cells[cell_idx].tile_cnt = 1;
      wfc->cells[cell_idx].sum_freqs = 0;
      wfc->cells[cell_idx].entropy = 0;
      wfc->collapsed_cell_cnt++;
      return 1;
    }
  }

  return 0;
}

static int wfc__next_cell(struct wfc *wfc)
{
  int min_idx = -1;
  double min_entropy = DBL_MAX;

  for (int i=0; i<wfc->cell_cnt; i++) {
    // Add small noise to break ties between tiles with the same entropy
    double entropy = wfc->cells[i].entropy + rand() / (100000.0 * RAND_MAX);
    if (wfc->cells[i].tile_cnt != 1 && entropy < min_entropy) {
      min_entropy = entropy;
      min_idx = i;
    }
  }

  return min_idx;
}

static void wfc__init_cells(struct wfc *wfc)
{
  int sum_freqs = 0.0;
  for (int i=0; i<wfc->tile_cnt; i++)
    sum_freqs += wfc->tiles[i].freq;
  wfc->sum_freqs = sum_freqs;

  double sum_plogp = 0.0;
  for (int i=0; i<wfc->tile_cnt; i++) {
    double p = ((double)wfc->tiles[i].freq) / sum_freqs;
    sum_plogp += p*log(p);
  }
  double entropy = -sum_plogp;

  for (int i=0; i<wfc->cell_cnt; i++) {
    wfc->cells[i].tile_cnt = wfc->tile_cnt;
    wfc->cells[i].sum_freqs = sum_freqs;
    wfc->cells[i].entropy = entropy;
    for (int j=0; j<wfc->tile_cnt; j++) {
      wfc->cells[i].tiles[j] = j;
    }
  }

  wfc->prop_cnt = 0;
}

// Allows to call wfc_run again
void wfc_init(struct wfc *wfc)
{
  wfc->seed = (unsigned int) time(NULL); // 1641743677
  srand(wfc->seed);
  wfc->collapsed_cell_cnt = 0;
  wfc__init_cells(wfc);
}

// max_collapse_cnt of -1 means no iteration number limit
//
// Return 0 on error (contradiction occurred)
int wfc_run(struct wfc *wfc, int max_collapse_cnt)
{
  //int cell_idx = (wfc->output_height / 2) * wfc->output_width + wfc->output_width / 2;
  int cell_idx = rand() % (wfc->output_height * wfc->output_width);

  while (1) {
    print_progress(wfc->collapsed_cell_cnt);

    if (!wfc__collapse(wfc, cell_idx)) {
      print_endprogress();
      return 0;
    }

    if (!wfc__propagate(wfc, cell_idx)) {
      print_endprogress();
      return 0;
    }

    cell_idx = wfc__next_cell(wfc);

    if (cell_idx == -1 || wfc->collapsed_cell_cnt == max_collapse_cnt) {
      break;
    }
  }

  print_progress(wfc->collapsed_cell_cnt);
  print_endprogress();

  return 1;
}

void wfc_destroy(struct wfc *wfc)
{
  wfc__destroy_cells(wfc->cells, wfc->cell_cnt);
  wfc__destroy_tiles(wfc->tiles, wfc->tile_cnt);
  wfc__destroy_allowed_tiles(wfc->allowed_tiles);
  wfc__destroy_props(wfc->props);
  free(wfc);
}

////////////////////////////////////////////////////////////////////////////////
//
// WFC: Overlapping method
//
////////////////////////////////////////////////////////////////////////////////

static void wfc__compute_allowed_tiles(int *allowed_tiles[4], struct wfc__tile *tiles, int tile_cnt)
{
  for (int d=0; d<4; d++) {
    for (int i=0; i<tile_cnt; i++) {
      for (int j=0; j<tile_cnt; j++) {
        //if (i==j)
        //  continue;
        allowed_tiles[d][i*tile_cnt + j] = wfc__img_cmpoverlap(tiles[i].image, tiles[j].image, d);
      }
    }
  }
}

// Return NULL on error
static struct wfc__tile *wfc__create_tiles_overlapping(struct wfc_image *image,
                                                int tile_width,
                                                int tile_height,
                                                int expand_image,
                                                int xflip_tiles,
                                                int yflip_tiles,
                                                int rotate_tiles,
                                                int *tile_cnt)
{
  int xcnt = image->width - tile_width + 1;
  int ycnt = image->height - tile_height + 1;

  if (expand_image) {
    xcnt = image->width;
    ycnt = image->height;
    image = wfc__img_expand(image, tile_width-1, tile_height-1);
    if (image == NULL)
      goto CLEANUP;
  }

  *tile_cnt = xcnt * ycnt;
  if (xflip_tiles)
    *tile_cnt *= 2;
  // xflip_tiles + rotate_tiles generate yflip_tiles
  if (!(xflip_tiles && rotate_tiles) && yflip_tiles)
    *tile_cnt *= 2;
  if (rotate_tiles)
    *tile_cnt *= 4;

  struct wfc__tile *tiles = wfc__create_tiles(*tile_cnt);
  if (tiles == NULL)
    goto CLEANUP;


  if (!wfc__add_overlapping_images(tiles, image, xcnt, ycnt, tile_width, tile_height))
    goto CLEANUP;

  int base_tile_cnt = xcnt * ycnt;
  if (xflip_tiles) {
    if (!wfc__add_flipped_images(tiles, base_tile_cnt, 0))
      goto CLEANUP;
    base_tile_cnt *= 2;
  }

  if (!(xflip_tiles && rotate_tiles) && yflip_tiles) {
    if (!wfc__add_flipped_images(tiles, base_tile_cnt, 1))
      goto CLEANUP;
    base_tile_cnt *= 2;
  }

  if (rotate_tiles) {
    if (!wfc__add_rotated_images(tiles, base_tile_cnt))
      goto CLEANUP;
    base_tile_cnt *= 4;
  }

  if (!wfc__remove_duplicate_tiles(&tiles, tile_cnt))
    goto CLEANUP;

  // If expand_image is set it means we've created a new image which
  // now needs to be destroyed
  wfc_img_destroy(expand_image ? image : NULL);

  return tiles;

CLEANUP:
  p("wfc__create_tiles_overlapping: error\n");
  wfc_img_destroy(expand_image ? image : NULL);
  wfc__destroy_tiles(tiles, base_tile_cnt);

  return NULL;
}

// Return NULL on error
struct wfc *wfc_overlapping(int output_width,
                            int output_height,
                            struct wfc_image *image,
                            int tile_width,
                            int tile_height,
                            int expand_input,
                            int xflip_tiles,
                            int yflip_tiles,
                            int rotate_tiles)
{
  struct wfc *wfc = malloc(sizeof(*wfc));
  if (wfc == NULL)
    goto CLEANUP;

  wfc->method = WFC_METHOD_OVERLAPPING;
  wfc->image = image;
  wfc->cells = NULL;
  wfc->tiles = NULL;
  wfc->props = NULL;
  wfc->output_width = output_width;
  wfc->output_height = output_height;
  wfc->cell_cnt = output_width * output_height;
  wfc->tile_width = tile_width;
  wfc->tile_height = tile_height;
  wfc->tile_cnt = 0;
  wfc->expand_input = expand_input;
  wfc->xflip_tiles = xflip_tiles;
  wfc->yflip_tiles = yflip_tiles;
  wfc->rotate_tiles = rotate_tiles;
  // use entropy
  // ...

  wfc->tiles = wfc__create_tiles_overlapping(wfc->image,
                                             wfc->tile_width,
                                             wfc->tile_height,
                                             wfc->expand_input,
                                             wfc->xflip_tiles,
                                             wfc->yflip_tiles,
                                             rotate_tiles,
                                             &wfc->tile_cnt);
  if (wfc->tiles == NULL)
    goto CLEANUP;

  if (!wfc__create_allowed_tiles(wfc->allowed_tiles, wfc->tile_cnt)) {
      goto CLEANUP;
    }
  wfc__compute_allowed_tiles(wfc->allowed_tiles, wfc->tiles, wfc->tile_cnt);

  wfc->cells = wfc__create_cells(wfc->cell_cnt, wfc->tile_cnt);
  if (wfc->cells == NULL)
    goto CLEANUP;

  wfc->props = wfc__create_props(wfc->cell_cnt);
  if (wfc->props == NULL)
    goto CLEANUP;

  wfc_init(wfc);

  return wfc;

 CLEANUP:
  p("wfc_overlapping: error\n");
  wfc_destroy(wfc);
  return NULL;
}

#endif // WFC_IMPLEMENTATION

////////////////////////////////////////////////////////////////////////////////
//
// Code graveyard
//

// TODO: make this into an option in wfc?
// int next_cell(struct system *s) {
//   int min_idx = -1;
//   int min_tile_cnt = INT_MAX;
//
//   for (int i=0; i<s->cell_cnt; i++) {
//     if (s->cells[i].tile_cnt != 1 && s->cells[i].tile_cnt < min_tile_cnt) {
//       min_tile_cnt = s->cells[i].tile_cnt;
//       min_idx = i;
//     }
//   }
//
//   return min_idx;
// }

// TODO: Move to wfc as an option
// void collapse(struct system *s, int cell_idx) {
//   double _01 = (double)rand() / RAND_MAX;
//   int idx = (int)(_01 * s->cells[cell_idx].tile_cnt);
//   s->cells[cell_idx].tiles[0] = s->cells[cell_idx].tiles[idx];
//   s->cells[cell_idx].tile_cnt = 1;
// }

/*******************************************************************************
  LICENSE:

  the MIT License (MIT)

  Copyright (c) 2020-2022 Krystian Samp

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
