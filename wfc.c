// Single-file Wave Function Collapse library / command-line tool.
// v.0.1 (pre-alpha) by Krystian Samp (samp.krystian at gmail.com)
//
// The library / command-line tool.
//
// Use as a library
// ----------------
//
// One file in your project should include wfc.c like this:
//
//   #define WFC_IMPLEMENTATION
//   #include "wfc.c"
//
// Other files in the project can also include wfc.c and use its functionality but
// they shouldn't define WFC_IMPLEMENTATION macro.
//
// Usage:
//
//   struct wfc *wfc = wfc_overlapping(
//     128,                 // Output image width in pixels
//     128,                 // Output image height in pixels
//     "input_image.png",   // Input image that will be cut into tiles
//     3,                   // Tile width in pixels
//     3,                   // Tile height in pixels
//     1,                   // Wrap input image on the right and bottom
//     1,                   // Add horizontal flips of all tiles
//     1,                   // Add vertical flips of all tiles
//     1                    // Add n*90deg rotations of all tiles
//   );
//
//   wfc_run(wfc, -1);      // Run Wave Function Collapse
//                          // -1 means no restriction on number of iterations
//   wfc_export(wfc, "output_image.png");
//   wfc_destroy(wfc);
//
//
// Use as a command-line tool
// --------------------------
//
//   make
//   ./wfc
//
//   Run ./wfc to see available options
//
//
// Development notes
// -----------------
//
// Available macros
//   WFC_TOOL                 // Include tool code (main)
//   WFC_IMPLEMENTATION       // Include implementation
//   WFC_DEBUG
//   WFC_NO_STB
//
//
// Log
// -----------------------------------------------------------------------------
// + (09.11.2020) Remove raylib dep, stb option, overlapping+tiled api prep
// + (31.10.2020) Command-line tool
// + (30.10.2020) Pre-alpha version
//
// - Update doc and tool help
// - Remove asserts when used as a lib
// - Handle failures in tile gen (e.g., 0 allowed neigbours)
// - Print seed in the summary
// - Add seed as an option
// - Add option for 'next_cell' that picks cell based on min_tile_cnt
// - Add option for 'collapse' that uni-randomly picks the tile
//

#ifndef WFC_H
#define WFC_H

struct wfc;

struct wfc_image {
  unsigned char *data;
  int component_cnt;
  int width;
  int height;
};

struct wfc *wfc_overlapping(int output_width,              // Output width in pixels
                            int output_height,             // Output height in pixels
                            struct wfc_image *image,       // Input image to be cut into tiles (takes ownership)
                            int tile_width,                // Tile width in pixels
                            int tile_height,               // Tile height in pixels
                            int expand_input,              // Wrap input image on right and bottom
                            int xflip_tiles,               // Add xflips of all tiles
                            int yflip_tiles,               // Add yflips of all tiles
                            int rotate_tiles);             // Add n*90deg rotations of all tiles

void wfc_init(struct wfc *wfc); // Resets wfc generation, wfc_run can be called again
int wfc_run(struct wfc *wfc, int max_collapse_cnt);
void wfc_export(struct wfc *wfc, const char *filename);
void wfc_destroy(struct wfc *wfc); // Also destroys the image

#endif // WFC_H

#ifdef WFC_TOOL
#define WFC_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif

#ifdef WFC_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifndef WFC_NO_STB
#include "stb_image.h"
#include "stb_image_write.h"
#endif

#ifdef WFC_DEBUG
#include <time.h>
#endif

#ifdef WFC_NO_STB

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

#else

#define print_progress(int_progress)
#define print_endprogress()
#define wfcassert(test)

#endif

enum wfc__direction {WFC_UP,WFC_DOWN,WFC_LEFT,WFC_RIGHT};
enum wfc__type {WFC_TYPE_OVERLAPPING, WFC_TYPE_TILED};

// Rules are stored in tiles
struct wfc__tile {
  struct wfc_image *image;
  int *allowed_tiles[4];       // An array of allowed tile indices in each
                               // direction. Typically tiles are allowed to each
                               // other if they overlap excluding the edges.
  int allowed_tile_cnt[4];
  int freq;                    // Relative frequency of the tile. Typically a
                               // count of tile occurrences in the input image.
                               // It affects the probability of the tile being
                               // selected when collapsing a cell.
};

struct wfc__cell {
  int *tiles;                  // Possible tiles in the cell (initially all)
  int tile_cnt;

  int sum_freqs;               // sum_* are cached values used to compute entropy
  int sum_log_freqs;
  double entropy;              // Typically we collapse cell with smallest entropy next
};

struct wfc__prop {
  int src_cell_idx;
  int dst_cell_idx;
  enum wfc__direction direction;
};

// One structure for overlapping and tiled models
struct wfc {
  enum wfc__type type;         // overlapping or tiled?
  int seed;

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

  /* output */

  int output_width;            // Output width in pixels
  int output_height;           // Output height in pixels
  struct wfc__cell *cells;     // One per output pixel
  int cell_cnt;                // width * height

  /* in-use */

  struct wfc__prop *props;     // Propagation updates
  int prop_cnt;
};

#ifdef WFC_DEBUG
const char *wfc__direction_strings[4] = {"up","down","left","right"};

void wfc__print_prop(struct wfc__prop *p, const char *prefix) {
  printf("%s%d -> %s -> %d\n", prefix, p->src_cell_idx, direction_strings[p->direction], p->dst_cell_idx);
}

void wfc__print_props(struct wfc__prop *p, int prop_cnt, const char *prefix) {
  for (int i=0; i<prop_cnt; i++) {
    print_prop(&(p[i]), prefix);
  }
}
#endif // WFC_DEBUG

////////////////////////////////////////////////////////////////////////////////
//
// Img helpers
//

#ifndef WFC_NO_STB

// non-0 on success, 0 on failure
int wfc_img_save(struct wfc_image *image, const char *filename)
{
  int len = strlen(filename);
  char extension[5];

  if (len < 4)
    goto UNKNOWNFORMAT;

  strcpy(extension, filename-4);
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

struct wfc_image *wfc_img_load(const char *filename)
{
  struct wfc_image *image = malloc(sizeof(*image));
  wfcassert(image != NULL);
  image->data = stbi_load(filename, &image->width, &image->height, &image->component_cnt, 0);
  wfcassert(image->data != NULL);
  return image;
}

#endif

struct wfc_image *wfc_img_copy(struct wfc_image *image) {
  struct wfc_image *copy = malloc(sizeof(*copy));
  wfcassert(copy!=NULL);
  copy->width = image->width;
  copy->height = image->height;
  copy->component_cnt = image->component_cnt;
  copy->data = malloc(sizeof(*copy->data) * copy->width*copy->height*copy->component_cnt);
  wfcassert(copy->data!=NULL);
  memcpy(copy->data, image->data, image->width*image->height*image->component_cnt);
  return copy;
}

struct wfc_image *wfc_img_create(int width, int height, int component_cnt)
{
  struct wfc_image *image = malloc(sizeof(*image));
  wfcassert(image!=NULL);
  image->width = width;
  image->height = height;
  image->component_cnt = component_cnt;
  image->data = malloc(sizeof(*image->data) * width * height * component_cnt);
  wfcassert(image->data!=NULL);
  return image;
}

void wfc_img_destroy(struct wfc_image *image) {
  if (image) {
    free(image->data);
    free(image);
  }
}

// This is a generic function but it's only used by the overlapping method
struct wfc_image *wfc__img_expand(struct wfc_image *image, int xexp, int yexp) {
  struct wfc_image *exp_image = wfc_img_create(image->width + xexp, image->height + yexp, image->component_cnt);

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
int wfc__img_cmpoverlap(struct wfc_image *a, struct wfc_image *b, enum wfc__direction direction) {
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

struct wfc_image *wfc__img_flip_horizontally(struct wfc_image *image) {
  struct wfc_image *copy = wfc_img_copy(image);
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

struct wfc_image *wfc__img_flip_vertically(struct wfc_image *image) {
  struct wfc_image *copy = wfc_img_copy(image);
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

struct wfc_image *wfc__img_rotate90(struct wfc_image *image, int n) {
  wfcassert(n>0);

  n %= 4;

  struct wfc_image *copy;
  if (n%2) {
    copy = wfc_img_create(image->height, image->width, image->component_cnt);
  } else {
    copy = wfc_img_create(image->width, image->height, image->component_cnt);
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
int wfc__img_cmp(struct wfc_image *a, struct wfc_image *b) {
  if (a->width!=b->width || a->height!=b->height || a->component_cnt!=b->component_cnt) {
    return 0;
  }
  return memcmp(a->data, b->data, a->width*a->height*a->component_cnt) == 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// WFC: Utils (Method-independent)
//

int wfc__nofunc_int(const char *func_name, const char *msg, ...)
{
  printf("%s: %s\n", func_name, msg);
  return -1;
}

void *wfc__nofunc_ptr(const char *func_name, const char *msg, ...)
{
  printf("%s: %s\n", func_name, msg);
  return NULL;
}

#ifndef WFC_NO_STB

// TODO: This is not shared code, it is specific to overlapping method
// dep: stb
void wfc_export(struct wfc *wfc, const char *filename)
{
  struct wfc_image *image = wfc_img_create(wfc->output_width, wfc->output_height, wfc->image->component_cnt);
  wfcassert(image != NULL);

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

  wfc_img_save(image, filename);
  wfc_img_destroy(image);
}

// dep: stb
void wfc_export_tiles(struct wfc *wfc, const char *path)
{
  char filename[128];
  for (int i=0; i<wfc->tile_cnt; i++) {
    sprintf(filename, "%s/%d.png", path, i);
    struct wfc__tile *tile = &wfc->tiles[i];
    wfc_img_save(tile->image, filename);
  }
}

#endif

// flip_direction: 0 - horizontal, 1 vertical
void wfc__add_flipped_tiles(struct wfc__tile **tiles, int *tile_cnt, int flip_direction)
{
  *tiles = realloc(*tiles, sizeof(**tiles) * (*tile_cnt)*2);
  wfcassert(*tiles != NULL);

  for (int i=0; i<*tile_cnt; i++) {
    (*tiles)[*tile_cnt + i].image = flip_direction == 0 ? wfc__img_flip_horizontally((*tiles)[i].image) : wfc__img_flip_vertically((*tiles)[i].image);
  }

  *tile_cnt = *tile_cnt * 2;
}

void wfc__add_rotated_tiles(struct wfc__tile **tiles, int *tile_cnt) {
  *tiles = realloc(*tiles, sizeof(**tiles) * (*tile_cnt)*4);
  assert(*tiles!=NULL);

  for (int i=0; i<*tile_cnt; i++) {
    for (int j=0; j<3; j++) {
      struct wfc_image *image;
      if (j==0) {
        image = (*tiles)[i].image;
      } else {
        image = (*tiles)[*tile_cnt + i*3 + j-1].image;
      }
      (*tiles)[*tile_cnt + i*3 + j].image = wfc__img_rotate90((*tiles)[i].image, j+1);
    }
  }

  *tile_cnt = *tile_cnt * 4;
}

// Remove duplicate tiles and calculate frequencies for the
// remaining unique tiles
void wfc__remove_duplicate_tiles(struct wfc__tile **tiles, int *tile_cnt)
{
  wfcassert(*tile_cnt);

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
      (*tiles)[unique_cnt] = (*tiles)[j];
      unique_cnt++;
    }
  }

  *tiles = realloc(*tiles, sizeof(**tiles) * unique_cnt);
  wfcassert(*tiles != NULL);

  *tile_cnt = unique_cnt;
}

// Assumes the tile fits in the image
struct wfc_image *create_tile_image(struct wfc_image *image, int x, int y, int tile_width, int tile_height) {
  struct wfc_image *tile_image = wfc_img_create(tile_width, tile_height, image->component_cnt);
  wfcassert(tile_image!=NULL);
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

////////////////////////////////////////////////////////////////////////////////
//
// WFC: Solve (Method-independent)
//

struct wfc__prop *wfc__create_props(int cell_cnt) {
  struct wfc__prop *props = malloc(sizeof(*props) * cell_cnt * 10);
  wfcassert(props != NULL);
  return props;
}

void wfc__destroy_props(struct wfc__prop *props) {
  free(props);
}

struct wfc__cell *wfc__create_cells(int cell_cnt, int tile_cnt) {
  struct wfc__cell *cells = malloc(sizeof(*cells) * cell_cnt);
  wfcassert(cells != NULL);

  for (int i=0; i<cell_cnt; i++) {
    cells[i].tiles = malloc(sizeof(*(cells[i].tiles)) * tile_cnt);
    wfcassert(cells[i].tiles);
  }

  return cells;
}

void wfc__destroy_cells(struct wfc__cell *cells, int cell_cnt) {
  for (int i=0; i<cell_cnt; i++) {
    free(cells[i].tiles);
  }
  free(cells);
}

void wfc__destroy_allowed_tiles(struct wfc__tile *tiles, int tile_cnt) {
  for (int i=0; i<tile_cnt; i++) {
    int directions[4] = {WFC_UP, WFC_DOWN, WFC_LEFT, WFC_RIGHT};
    for (int d_idx=0; d_idx<4; d_idx++) {
      free(tiles[i].allowed_tiles[directions[d_idx]]);
    }
  }
}

void wfc__destroy_tiles(struct wfc__tile *tiles, int tile_cnt) {
  wfc__destroy_allowed_tiles(tiles, tile_cnt);
  for (int i=0; i<tile_cnt; i++) {
    wfc_img_destroy(tiles[i].image);
  }
  free(tiles);
}

void wfc__add_prop(struct wfc *wfc, int src_cell_idx, int dst_cell_idx, enum wfc__direction direction)
{
  wfcassert(src_cell_idx >= 0);
  wfcassert(src_cell_idx < wfc->cell_cnt);
  wfcassert(dst_cell_idx >= 0);
  wfcassert(dst_cell_idx < wfc->cell_cnt);

  struct wfc__prop *p = &( wfc->props[wfc->prop_cnt] );
  (wfc->prop_cnt)++;;
  p->src_cell_idx = src_cell_idx;
  p->dst_cell_idx = dst_cell_idx;
  p->direction = direction;
}

// add prop to update cell above the cell_idx
void wfc__add_prop_up(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx - wfc->output_width >= 0) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx - wfc->output_width, WFC_UP);
  }
}

void wfc__add_prop_down(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx + wfc->output_width < wfc->cell_cnt) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx + wfc->output_width, WFC_DOWN);
  }
}

void wfc__add_prop_left(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx % wfc->output_width != 0) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx - 1, WFC_LEFT);
  }
}

void wfc__add_prop_right(struct wfc *wfc, int src_cell_idx)
{
  if (src_cell_idx % wfc->output_width != wfc->output_width - 1) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx + 1, WFC_RIGHT);
  }
}

// Return 1 if the tiles are allowed next to each other in the specified direction. 0 otherwise.
int wfc__tiles_allowed(struct wfc *wfc, int src_tile_idx, int dst_tile_idx, enum wfc__direction d) {
  int *allowed_tiles = wfc->tiles[ src_tile_idx ].allowed_tiles[d];
  int allowed_tile_cnt = wfc->tiles[ src_tile_idx ].allowed_tile_cnt[d];
  for (int i=0; i<allowed_tile_cnt; i++) {
    if (dst_tile_idx == allowed_tiles[i]) {
      return 1;
    }
  }
  return 0;
}

// Does the cell enable tile in the direction?
int wfc__tile_enabled(struct wfc *wfc, int tile_idx, int cell_idx, enum wfc__direction d)
{
  struct wfc__cell *cell = &( wfc->cells[cell_idx] );

  // Tile is enabled if any of the cell's tiles allowes it in
  // the specified diretion
  for (int i=0; i<cell->tile_cnt; i++) {
    int cell_tile_idx = cell->tiles[i];

    if (wfc__tiles_allowed(wfc, cell_tile_idx, tile_idx, d)) {
      return 1;
    }
  }
  return 0;
}


// Updates tiles in the destination cell to those that are allowed by the source cell
// and propagate updates
//
// -1 if contradiction occurred
//  0 on success
int wfc__propagate_prop(struct wfc *wfc, struct wfc__prop *p) {
  int new_cnt = 0;

  struct wfc__cell *dst_cell = &( wfc->cells[ p->dst_cell_idx ] );

  // Go through all destination tiles and check whether they are enabled by the source cell
  for (int i=0; i<dst_cell->tile_cnt; i++) {
    int possible_dst_tile_idx = dst_cell->tiles[i];

    // If a destination tile is enabled by the source cell, keep it
    if (wfc__tile_enabled(wfc, possible_dst_tile_idx, p->src_cell_idx, p->direction)) {
      dst_cell->tiles[new_cnt] = possible_dst_tile_idx;
      new_cnt++;
    } else {
      int freq = wfc->tiles[possible_dst_tile_idx].freq;
      dst_cell->sum_freqs -= freq;
      dst_cell->sum_log_freqs -= freq * log(freq);
      //printf("sum_freqs: %d\n", dst_cell->sum_freqs);
      dst_cell->entropy = log(dst_cell->sum_freqs) - dst_cell->sum_log_freqs / dst_cell->sum_freqs;
    }
  }

  assert(new_cnt != 0);
  if (!new_cnt) {
    return -1;
  }

  if (dst_cell->tile_cnt != new_cnt) {
    if (p->direction != WFC_DOWN) wfc__add_prop_up(wfc, p->dst_cell_idx);
    if (p->direction != WFC_UP) wfc__add_prop_down(wfc, p->dst_cell_idx);
    if (p->direction != WFC_RIGHT) wfc__add_prop_left(wfc, p->dst_cell_idx);
    if (p->direction != WFC_LEFT) wfc__add_prop_right(wfc, p->dst_cell_idx);
  }

  dst_cell->tile_cnt = new_cnt;

  return 0;
}

// -1 if contradiction occurred
//  0 on success
int wfc__propagate(struct wfc *wfc, int cell_idx) {
  wfc->prop_cnt = 0;

  wfc__add_prop_up(wfc, cell_idx);
  wfc__add_prop_down(wfc, cell_idx);
  wfc__add_prop_left(wfc, cell_idx);
  wfc__add_prop_right(wfc, cell_idx);

  int cnt = 0;
  for (int i=0; i<wfc->prop_cnt; i++) {
    struct wfc__prop *p = &( wfc->props[i] );
    if (wfc__propagate_prop(wfc, p) == -1) {
      return -1;
    }
    cnt++;
  }

  return 0;
}

// -1 if contradiction occurred
//  0 on success
int wfc__collapse(struct wfc *wfc, int cell_idx)
{
  int remaining = rand() % wfc->cells[cell_idx].sum_freqs;
  for (int i=0; i<wfc->cells[cell_idx].tile_cnt; i++) {
    int freq = wfc->tiles[ wfc->cells[cell_idx].tiles[i] ].freq;
    if (remaining >= freq) {
      remaining -= freq;
    } else {
      wfc->cells[cell_idx].tiles[0] = wfc->cells[cell_idx].tiles[i];
      wfc->cells[cell_idx].tile_cnt = 1;
      wfc->cells[cell_idx].entropy = 0;
      return 0;
    }
  }

  return -1;
}

int wfc__next_cell(struct wfc *wfc) {
  int min_idx = -1;
  double min_entropy = DBL_MAX;

  for (int i=0; i<wfc->cell_cnt; i++) {
    if (wfc->cells[i].tile_cnt != 1 && wfc->cells[i].entropy < min_entropy) {
      min_entropy = wfc->cells[i].entropy;
      min_idx = i;
    }
  }

  return min_idx;
}

// -1 if generation was unsuccessful (probably contradiction encountered)
//  0 on success
//
// collapse_cnt can be used to restrict the max number of collapses. -1
// removes any restriction
void wfc__init_cells(struct wfc *wfc)
{
  int sum_freqs = 0;
  int sum_log_freqs = 0;
  for (int i=0; i<wfc->tile_cnt; i++) {
    int freq = wfc->tiles[i].freq;
    sum_freqs += freq;
    sum_log_freqs += freq * log(freq);
  }
  double entropy = log(sum_freqs) - sum_log_freqs/sum_freqs;

  for (int i=0; i<wfc->cell_cnt; i++) {
    wfc->cells[i].tile_cnt = wfc->tile_cnt;
    wfc->cells[i].sum_freqs = sum_freqs;
    wfc->cells[i].sum_log_freqs = sum_log_freqs;
    wfc->cells[i].entropy = entropy;
    for (int j=0; j<wfc->tile_cnt; j++) {
      wfc->cells[i].tiles[j] = j;
    }
  }

  wfc->prop_cnt = 0;
}

// TODO: corrent to 0 on success
int wfc_run(struct wfc *wfc, int max_collapse_cnt)
{
  int cnt=0;
  int cell_idx = (wfc->output_height / 2) * wfc->output_width + wfc->output_width / 2;

  while (1) {
    print_progress(cnt);

    if (wfc__collapse(wfc, cell_idx) == -1) {
      return -1;
    }

    if (wfc__propagate(wfc, cell_idx) == -1) {
      return -1;
    }

    cell_idx = wfc__next_cell(wfc);
    cnt++;

    if (cell_idx == -1 || cnt == max_collapse_cnt) {
      break;
    }
  }

  print_endprogress();

  return 0;
}

void wfc_destroy(struct wfc *wfc)
{
  wfc__destroy_cells(wfc->cells, wfc->cell_cnt);
  wfc__destroy_tiles(wfc->tiles, wfc->tile_cnt);
  wfc__destroy_props(wfc->props);
  wfc_img_destroy(wfc->image);
  free(wfc);
}

////////////////////////////////////////////////////////////////////////////////
//
// WFC: Overlapping method
//

void wfc__create_allowed_tiles(struct wfc__tile *tiles, int tile_cnt) {
  int directions[4] = {WFC_UP,WFC_DOWN,WFC_LEFT,WFC_RIGHT};

  for (int d_idx=0; d_idx<4; d_idx++) {
    enum wfc__direction d = directions[d_idx];

    for (int i=0; i<tile_cnt; i++) {

      struct wfc__tile *t = &( tiles[i] );
      t->allowed_tiles[d] = malloc(sizeof(*(tiles[i].allowed_tiles[d])) * tile_cnt);
      t->allowed_tile_cnt[d] = 0;
      assert(t->allowed_tiles[d]);

      for (int j=0; j<tile_cnt; j++) {
        /* if (i==j) { */
        /*   continue; */
        /* } */

        if (wfc__img_cmpoverlap(tiles[i].image, tiles[j].image, d)) {
          t->allowed_tiles[d][ t->allowed_tile_cnt[d] ] = j;
          t->allowed_tile_cnt[d]++;
        }
      }
    }
  }
}

struct wfc__tile *wfc__create_tiles_overlapping(struct wfc_image *image,
                                                int tile_width,
                                                int tile_height,
                                                int expand_image,
                                                int xflip_tiles,
                                                int yflip_tiles,
                                                int rotate_tiles,
                                                int *tile_cnt)
{

  int width = image->width - tile_width + 1;
  int height = image->height - tile_height + 1;

  if (expand_image) {
    width = image->width;
    height = image->height;
    image = wfc__img_expand(image, tile_width-1, tile_height-1);
  }

  *tile_cnt = width * height;

  struct wfc__tile *tiles = malloc(sizeof(*tiles) * (*tile_cnt));
  assert(tiles!=NULL);

  for (int y=0; y<height; y++) {
    for (int x=0; x<width; x++) {
      struct wfc__tile *tile = &( tiles[y*width + x] );
      tile->image = create_tile_image(image, x, y, tile_width, tile_height);
      //tile->allowed_tiles = NULL;
      tile->freq = 1;
    }
  }

  if (xflip_tiles) wfc__add_flipped_tiles(&tiles, tile_cnt, 0);
  if (yflip_tiles) wfc__add_flipped_tiles(&tiles, tile_cnt, 1);
  if (rotate_tiles) wfc__add_rotated_tiles(&tiles, tile_cnt);

  wfc__remove_duplicate_tiles(&tiles, tile_cnt);
  wfc__create_allowed_tiles(tiles, *tile_cnt);

  // If expand_image is set it means we've created a new image which
  // now needs to be destroyed
  if (expand_image) {
    wfc_img_destroy(image);
  }

  return tiles;
}

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
  assert(wfc!=NULL);

  wfc->type = WFC_TYPE_OVERLAPPING;
  wfc->image = image;
  wfc->output_width = output_width;
  wfc->output_height = output_height;
  wfc->cell_cnt = output_width * output_height;
  wfc->tile_width = tile_width;
  wfc->tile_height = tile_height;
  wfc->tile_cnt = -1; // This is set by wfc__create_tiles_overlapping below
  wfc->expand_input = expand_input;
  wfc->xflip_tiles = xflip_tiles;
  wfc->yflip_tiles = yflip_tiles;
  wfc->rotate_tiles = rotate_tiles;
  wfc->seed = 1234;
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
  wfc->cells = wfc__create_cells(wfc->cell_cnt, wfc->tile_cnt);
  wfc->props = wfc__create_props(wfc->cell_cnt);
  wfc->prop_cnt = 0; // unnecessary

  wfc__init_cells(wfc);

  return wfc;
}

#ifdef WFC_TOOL

////////////////////////////////////////////////////////////////////////////////
//
// Command-line tool
//

void print_summary(struct wfc *wfc, const char *input_image, const char *output_image) {
  //printf("\nWFC setup summary\n");
  printf("\n");
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

void usage(const char *program_name, int exit_code) {
  if (exit_code != EXIT_SUCCESS) {
    printf("Wrong input\n\n");
  }

  printf("Usage: %s [OPTION] input_image output_image\n\n", program_name);
  printf("\
Procedurally generate output_image based on the input_image\n\
using Wave Function Collapse algorithm.\n\
\n\
");
  printf("\
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
Example:\n\
\n\
./wfc -w 128 -h 128 -W 3 -H 3 -e 1 -h 1 -v 1 -r 1 tiles/plant.png output.png\n\
");

  exit(exit_code);
}

int arg_num(int argc, const char **argv, int *i, const char *short_name, const char *long_name, int *num) {
  int n;
  char str[128];

  sprintf(str, "-%s", short_name);
  if (strlen(argv[*i])==2 && memcmp(argv[*i], str, 2)==0) {
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

void read_args(int argc, const char **argv, const char **input, const char **output, int *width, int *height, int *tile_width, int *tile_height, int *expand_image, int *xflip_tiles, int *yflip_tiles, int *rotate_tiles) {
  if (argc<2) {
    usage(argv[0], EXIT_FAILURE);
  }

  int i=1;
  while (i<argc) {
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

    *input = argv[i];
    *output = argv[i+1];
    return;
  };
}

int main(int argc, const char **argv) {
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
  struct wfc *wfc = wfc_overlapping(output_width,
                                    output_height,
                                    image,
                                    tile_width,
                                    tile_height,
                                    expand_input,
                                    xflip_tiles,
                                    yflip_tiles,
                                    rotate_tiles);

  /* wfc_export_tiles(wfc, "tmp"); */
  print_summary(wfc, input_filename, output_filename);

  wfc_run(wfc, -1);
  wfc_img_destroy(image);
  wfc_export(wfc, output_filename);

  return 0;
}

#endif // WFC_TOOL

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
