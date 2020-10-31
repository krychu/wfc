// Single-file Wave Function Collapse library / command-line tool.
// v.0.1 (pre-alpha) by Krystian Samp (samp.krystian at gmail.com)
//
// The library / command-line tool currently requires raylib.
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
//   struct wfc *wfc = wfc_create(
//     128,                 // Output image width in pixels
//     128,                 // Output image height in pixels
//     "input_image.png",   // Input image
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
//   WFC_USE_RAYLIB           // raylib is currently required
//   WFC_USE_STB              // Planned
//
//
// Log
// -----------------------------------------------------------------------------
// + (31.10.2020) Command-line tool
// + (30.10.2020) Pre-alpha version
//
// - Remove asserts when used as a lib
// - Handle failures in tile gen (e.g., 0 allowed neigbours)
// - Print seed in the summary
// - Add seed as an option
//

#define WFC_USE_RAYLIB // Currently wfc requires raylib

#ifndef KHU_WFC_H
#define KHU_WFC_H

struct wfc;
struct wfc *wfc_create(
                       int width,                     // Output width in pixels
                       int height,                    // Output height in pixels
                       const char *input_filename,    // Input image that will be cut into tiles
                       int tile_width,                // Tile width in pixels
                       int tile_height,               // Tile height in pixels
                       int expand_image,              // Wrap input image on right and bottom
                       int xflip_tiles,               // Add xflips of all tiles
                       int yflip_tiles,               // Add yflips of all tiles
                       int rotate_tiles               // Add n*90deg rotations of all tiles
                       );

void wfc_init(struct wfc *wfc); // Resets wfc generation, wfc_run can be called again
int wfc_run(struct wfc *wfc, int max_collapse_cnt);
#ifdef WFC_USE_RAYLIB
void wfc_export(struct wfc *wfc, const char *filename);
#endif
void wfc_destroy(struct wfc *wfc);

#endif // KHU_WFC_H

#ifdef WFC_TOOL
#define WFC_IMPLEMENTATION
#endif

#ifdef WFC_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <assert.h>

#ifdef WFC_DEBUG
#include <time.h>
#endif

#ifdef WFC_USE_RAYLIB
#include "raylib.h"
#endif

enum wfc__direction {WFC_UP,WFC_DOWN,WFC_LEFT,WFC_RIGHT};

struct wfc__tile {
#ifdef WFC_USE_RAYLIB
  Image image;
  Color *colors;
#endif
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

  int sum_freqs;               // sum_* are cached values used to compute
                               // entropy.
  int sum_log_freqs;
  double entropy;              // Typically we pick a cell with smalest entropy
                               // to collapse next.
};

struct wfc__prop {
  int src_cell_idx;
  int dst_cell_idx;
  enum wfc__direction direction;
};

struct wfc {
  int width;                   // Output width in pixels
  int height;                  // Output height in pixels

  int tile_width;              // Tile width in pixels
  int tile_height;             // Tile height in pixels

  struct wfc__tile *tiles;     // All available tiles
  int tile_cnt;
  struct wfc__cell *cells;     // One per output pixel
  int cell_cnt;                // width * height

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
#endif

#ifdef WFC_USE_RAYLIB

// 1 yes
// 0 no
int wfc__images_overlap(Image src_image, Image dst_image, enum wfc__direction direction) {
  int src_offx, src_offy, dst_offx, dst_offy, width, height;

  switch (direction) {
  case WFC_UP:
    src_offx = 0; src_offy = 0;
    dst_offx = 0; dst_offy = 1;
    width = src_image.width;
    height = src_image.height-1;
    break;
  case WFC_DOWN:
    src_offx = 0; src_offy = 1;
    dst_offx = 0; dst_offy = 0;
    width = src_image.width;
    height = src_image.height-1;
    break;
  case WFC_LEFT:
    src_offx = 0; src_offy = 0;
    dst_offx = 1; dst_offy = 0;
    width = src_image.width-1;
    height = src_image.height;
    break;
  case WFC_RIGHT:
    src_offx = 1; src_offy = 0;
    dst_offx = 0; dst_offy = 0;
    width = src_image.width-1;
    height = src_image.height;
    break;
  default:
    printf("wfc error: wrong direction (%d)\n", direction);
    return 0;
  };

  Color *src_colors = GetImageData(src_image);
  Color *dst_colors = GetImageData(dst_image);

  for (int y=0; y<height; y++) {
    int src_y = src_offy + y;
    int dst_y = dst_offy + y;
    for (int x=0; x<width; x++) {
      int src_x = src_offx + x;
      int dst_x = dst_offx + x;
      Color c1 = src_colors[src_y * src_image.width + src_x];
      Color c2 = dst_colors[dst_y * src_image.width + dst_x];

      if (c1.r != c2.r || c1.g != c2.g || c1.b != c2.b || c1.a != c2.a) {
        return 0;
      }
    }
  }

  return 1;
}

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

        if (wfc__images_overlap(tiles[i].image, tiles[j].image, d)) {
          t->allowed_tiles[d][ t->allowed_tile_cnt[d] ] = j;
          t->allowed_tile_cnt[d]++;
        }
      }
    }
  }
}

int wfc__same_tiles(struct wfc__tile *a, struct wfc__tile *b) {
  int pixel_cnt = a->image.width * a->image.height;
  for (int i=0; i<pixel_cnt; i++) {
    if (a->colors[i].r != b->colors[i].r ||
        a->colors[i].g != b->colors[i].g ||
        a->colors[i].b != b->colors[i].b ||
        a->colors[i].a != b->colors[i].a) {
      return 0;
    }
  }
  return 1;
}

// flip_direction: 0 - horizontal, 1 vertical
void wfc__add_flipped_tiles(struct wfc__tile **tiles, int *tile_cnt, int flip_direction) {
  *tiles = realloc(*tiles, sizeof(**tiles) * (*tile_cnt)*2);
  assert(*tiles);

  for (int i=0; i<*tile_cnt; i++) {
    Image flipped = ImageCopy((*tiles)[i].image);
    if (flip_direction) {
      ImageFlipVertical(&flipped);
    } else {
      ImageFlipHorizontal(&flipped);
    }
    (*tiles)[i+(*tile_cnt)].image = flipped;
    (*tiles)[i+(*tile_cnt)].colors = GetImageData(flipped);
  }

  *tile_cnt = *tile_cnt * 2;
}

void wfc__add_rotated_tiles(struct wfc__tile **tiles, int *tile_cnt) {
  *tiles = realloc(*tiles, sizeof(**tiles) * (*tile_cnt)*4);
  assert(*tiles);

  for (int i=0; i<*tile_cnt; i++) {
    for (int j=0; j<3; j++) {
      Image image;
      if (j==0) {
        image = (*tiles)[i].image;
      } else {
        image = (*tiles)[*tile_cnt + i*3 + j-1].image;
      }
      Image rotated = ImageCopy(image);
      ImageRotateCW(&rotated);
      (*tiles)[*tile_cnt + i*3 + j].image = rotated;
      (*tiles)[*tile_cnt + i*3 + j].colors = GetImageData(rotated);
    }
  }

  *tile_cnt = *tile_cnt * 4;
}

// Remove duplicate tiles and calculate frequencies for the
// remaining unique tiles
void wfc__remove_duplicate_tiles(struct wfc__tile **tiles, int *tile_cnt) {
  assert(*tile_cnt);

  int unique_cnt = 1;
  for (int j=1; j<*tile_cnt; j++) {
    int unique = 1;
    for (int k=0; k<unique_cnt; k++) {
      if (wfc__same_tiles(&((*tiles)[j]), &((*tiles)[k]))) {
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
  assert(*tiles);

  *tile_cnt = unique_cnt;
}

struct wfc__tile *wfc__create_tiles(const char *filename, int tile_width, int tile_height, int *tile_cnt, int expand_image, int xflip_tiles, int yflip_tiles, int rotate_tiles) {
  Image image = LoadImage(filename);

  // TODO: Move to separate function
  if (expand_image) {
    Image image_ext = GenImageColor(image.width + tile_width-1, image.height + tile_height-1, RAYWHITE);

    Color *colors = GetImageData(image);

    for (int y=0; y<image_ext.height; y++) {
      for (int x=0; x<image_ext.width; x++) {
        int image_x = x % image.width;
        int image_y = y % image.height;
        ImageDrawPixel(&image_ext, x, y, colors[image_y * image.width + image_x]);
      }
    }

    free(colors);
    UnloadImage(image);
    image = image_ext;
  }

  int tile_xcnt = image.width - tile_width + 1;
  int tile_ycnt = image.height - tile_height + 1;
  *tile_cnt = tile_xcnt * tile_ycnt;

  struct wfc__tile *tiles = malloc(sizeof(*tiles) * (*tile_cnt));
  assert(tiles != NULL);

  for (int y=0; y<tile_ycnt; y++) {
    for (int x=0; x<tile_xcnt; x++) {
      struct wfc__tile *tile = &( tiles[y*tile_ycnt + x] );
      tile->image = ImageFromImage(image, (Rectangle){x, y, tile_width, tile_height});
      tile->colors = GetImageData(tile->image);
      tile->freq = 1;
      assert(tile->colors);
    }
  }

  if (xflip_tiles) {
    wfc__add_flipped_tiles(&tiles, tile_cnt, 0);
  }
  if (yflip_tiles) {
    wfc__add_flipped_tiles(&tiles, tile_cnt, 1);
  }
  if (rotate_tiles) {
    wfc__add_rotated_tiles(&tiles, tile_cnt);
  }
  wfc__remove_duplicate_tiles(&tiles, tile_cnt);
  wfc__create_allowed_tiles(tiles, *tile_cnt);

  return tiles;
}

void wfc_export(struct wfc *wfc, const char *filename) {
  Image output = GenImageColor(wfc->width, wfc->height, RAYWHITE);

  for (int y=0; y<wfc->height; y++) {
    for (int x=0; x<wfc->width; x++) {
      struct wfc__cell *cell = &( wfc->cells[y * wfc->width + x] );

      double r=0, g=0, b=0, a=0;
      for (int i=0; i<cell->tile_cnt; i++) {
        struct wfc__tile *tile = &( wfc->tiles[ cell->tiles[i] ] );
        r += tile->colors[0].r;
        g += tile->colors[0].g;
        b += tile->colors[0].b;
        a += tile->colors[0].a;
      }
      Color color;
      color.r = r / cell->tile_cnt;
      color.g = g / cell->tile_cnt;
      color.b = b / cell->tile_cnt;
      color.a = a / cell->tile_cnt;

      ImageDrawPixel(&output, x, y, color);
    }
  }

  ExportImage(output, filename);
}

void wfc__export_cnts(struct wfc *wfc, const char *filename) {
  Image output = GenImageColor(wfc->width, wfc->height, RAYWHITE);
  for (int y=0; y<wfc->height; y++) {
    for (int x=0; x<wfc->width; x++) {
      struct wfc__cell *cell = &( wfc->cells[y * wfc->width + x] );
      int c = ((double)cell->tile_cnt / wfc->tile_cnt) * 255;
      ImageDrawPixel(&output, x, y, (Color){c, c, c, 255});
    }
  }

  ExportImage(output, filename);
}
#endif // WFC_USE_RAYLIB

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
    UnloadImage(tiles[i].image);
  }
  free(tiles);
}

void wfc__export_tiles(const char *path, struct wfc__tile *tiles, int tile_cnt) {
  char filename[128];
  for (int i=0; i<tile_cnt; i++) {
    sprintf(filename, "%s/%d.png", path, i);
    ExportImage(tiles[i].image, filename);
  }
}

// -1 if contradiction occurred
//  0 on success
int wfc__collapse(struct wfc *wfc, int cell_idx) {
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

// TODO: Move to wfc as an option
// void collapse(struct system *s, int cell_idx) {
//   double _01 = (double)rand() / RAND_MAX;
//   int idx = (int)(_01 * s->cells[cell_idx].tile_cnt);
//   s->cells[cell_idx].tiles[0] = s->cells[cell_idx].tiles[idx];
//   s->cells[cell_idx].tile_cnt = 1;
// }

void wfc__add_prop(struct wfc *wfc, int src_cell_idx, int dst_cell_idx, enum wfc__direction direction) {
#if defined(WFC_DEBUG) || defined(WFC_TOOL)
  assert(src_cell_idx >= 0);
  assert(src_cell_idx < wfc->cell_cnt);
  assert(dst_cell_idx >= 0);
  assert(dst_cell_idx < wfc->cell_cnt);
#endif

  struct wfc__prop *p = &( wfc->props[wfc->prop_cnt] );
  (wfc->prop_cnt)++;;
  p->src_cell_idx = src_cell_idx;
  p->dst_cell_idx = dst_cell_idx;
  p->direction = direction;
}

// add prop to update cell above the cell_idx
void wfc__add_prop_up(struct wfc *wfc, int src_cell_idx) {
  if (src_cell_idx - wfc->width >= 0) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx - wfc->width, WFC_UP);
  }
}

void wfc__add_prop_down(struct wfc *wfc, int src_cell_idx) {
  if (src_cell_idx + wfc->width < wfc->cell_cnt) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx + wfc->width, WFC_DOWN);
  }
}

void wfc__add_prop_left(struct wfc *wfc, int src_cell_idx) {
  if (src_cell_idx % wfc->width != 0) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx - 1, WFC_LEFT);
  }
}

void wfc__add_prop_right(struct wfc *wfc, int src_cell_idx) {
  if (src_cell_idx % wfc->width != wfc->width - 1) {
    wfc__add_prop(wfc, src_cell_idx, src_cell_idx + 1, WFC_RIGHT);
  }
}

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
int wfc__tile_enabled(struct wfc *wfc, int tile_idx, int cell_idx, enum wfc__direction d) {
  struct wfc__cell *cell = &( wfc->cells[cell_idx] );

  // Go through all tiles in the cell and check whether they allow tile_idx in the specified direction
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
      dst_cell->entropy = log(dst_cell->sum_freqs) - dst_cell->sum_log_freqs / dst_cell->sum_freqs;
    }
  }

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

#if defined(WFC_DEBUG)
  time_t t = time(NULL);
#endif

  int cnt = 0;
  for (int i=0; i<wfc->prop_cnt; i++) {
    struct wfc__prop *p = &( wfc->props[i] );
    if (wfc__propagate_prop(wfc, p) == -1) {
      return -1;
    }
    cnt++;
  }

#if defined(WFC_DEBUG)
    printf("%d propagate_prop calls: %ld seconds\n", cnt, time(NULL) - t);
#endif

    return 0;
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

// -1 if generation was unsuccessful (probably contradiction encountered)
//  0 on success
//
// collapse_cnt can be used to restrict the max number of collapses. -1
// removes any restriction
int wfc_run(struct wfc *wfc, int max_collapse_cnt) {
  int cnt = 0;
  int cell_idx = (wfc->height/2)*wfc->width + wfc->width/2;

  while (1) {
#if defined(WFC_DEBUG) || defined(WFC_TOOL)
      printf("\r%d collapsed", cnt);
      fflush(stdout);
#endif

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

#if defined(WFC_DEBUG) || defined(WFC_TOOL)
  printf("\n");
#endif

  return 0;
}

struct wfc *wfc_create(int width, int height, const char *tile_filename, int tile_width, int tile_height, int expand_image, int xflip_tiles, int yflip_tiles, int rotate_tiles) {
  struct wfc *wfc = malloc(sizeof(*wfc));
  assert(wfc!=NULL);

  wfc->width = width;
  wfc->height = height;
  wfc->cell_cnt = width * height;
  wfc->tile_width = tile_width;
  wfc->tile_height = tile_height;

  wfc->tiles = wfc__create_tiles(tile_filename, tile_width, tile_height, &(wfc->tile_cnt), expand_image, xflip_tiles, yflip_tiles, rotate_tiles);

  wfc->cells = malloc(sizeof(*(wfc->cells)) * wfc->cell_cnt);
  assert(wfc->cells!=NULL);
  for (int i=0; i<wfc->cell_cnt; i++) {
    wfc->cells[i].tiles = malloc(sizeof(*(wfc->cells[i].tiles)) * wfc->tile_cnt);
    assert(wfc->cells[i].tiles);
  }

  wfc->props = malloc(sizeof(*(wfc->props)) * wfc->cell_cnt * 10);
  assert(wfc->props!=NULL);
  wfc->prop_cnt = 0;

  wfc_init(wfc);

  return wfc;
}

void wfc_destroy(struct wfc *wfc) {
  free(wfc->props);
  for (int i=0; i<wfc->cell_cnt; i++) {
    free(wfc->cells[i].tiles);
  }
  free(wfc->cells);
  wfc__destroy_tiles(wfc->tiles, wfc->tile_cnt);
  free(wfc);
}

void wfc_init(struct wfc *wfc) {
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

#ifdef WFC_TOOL
void print_summary(struct wfc *wfc, const char *input_image, const char *output_image, int expand_image, int xflip_tiles, int yflip_tiles, int rotate_tiles) {
  //printf("\nWFC setup summary\n");
  printf("\n");
  printf("input image:          %s\n", input_image);
  printf("tile size:            %dx%d\n", wfc->tile_width, wfc->tile_height);
  printf("expand input:         %d\n", expand_image);
  printf("xflip tiles:          %d\n", xflip_tiles);
  printf("yflip tiles:          %d\n", yflip_tiles);
  printf("rotate tiles:         %d\n", rotate_tiles);
  printf("tile count:           %d\n", wfc->tile_cnt);
  printf("\n");
  printf("output image:         %s\n", output_image);
  printf("output size:          %dx%d\n", wfc->width, wfc->height);
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
  const char *input_image;
  const char *output_image;
  int width = 64;
  int height = 64;
  int tile_width = 3;
  int tile_height = 3;
  int expand_image = 1;
  int xflip_tiles = 0;
  int yflip_tiles = 0;
  int rotate_tiles = 0;

  read_args(argc, argv, &input_image, &output_image, &width, &height, &tile_width, &tile_height, &expand_image, &xflip_tiles, &yflip_tiles, &rotate_tiles);

  struct wfc *wfc = wfc_create(
                               width, height,               // Output resolution in pixels
                               input_image,                 // Input image to cut the tiles from
                               tile_width, tile_height,     // Tile resolution in pixels
                               expand_image,                // Wrap input image on the right and bottom
                               xflip_tiles,                 // Add xflips of all tiles
                               yflip_tiles,                 // Add yflips of all tiles
                               rotate_tiles                 // Add n*90deg rotations of all tiles
                               );

  print_summary(wfc, input_image, output_image, expand_image, xflip_tiles, yflip_tiles, rotate_tiles);

  wfc_run(wfc, -1);
  wfc_export(wfc, output_image);
  wfc_destroy(wfc);
}
#endif  // WFC_TOOL

#endif
