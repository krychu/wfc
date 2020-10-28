#ifndef _WFC
#define _WFC

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "raylib.h"

enum direction {
                UP,
                DOWN,
                LEFT,
                RIGHT
};

enum direction opposite_direction[4] = {DOWN, UP, RIGHT, LEFT};

const char *direction_strings[4] = {
                                    "up",
                                    "down",
                                    "left",
                                    "right"
};

struct tile {
  Image image;
  int *allowed_tiles[4];       // An array of allowed tiles in each direction
  int allowed_tile_cnt[4];
};

struct cell {
  int *tiles;                  // Possible tiles in the cell
  int tile_cnt;
};

struct prop {
  int src_cell_idx;
  int dst_cell_idx;
  enum direction direction;
};

struct system {
  int width;                   // output width in pixels
  int height;                  // output height in pixels

  int tile_width;              // tile width in pixels
  int tile_height;             // tile height in pixels

  struct tile *tiles;          // All tiles
  int tile_cnt;
  struct cell *cells;          // One per output pixel
  int cell_cnt;                // width * height

  struct prop *props;          // propagation entries
  int prop_cnt;
};

void print_prop(struct prop *p, const char *prefix) {
  printf("%s%d -> %s -> %d\n", prefix, p->src_cell_idx, direction_strings[p->direction], p->dst_cell_idx);
}

void print_props(struct prop *p, int prop_cnt, const char *prefix) {
  for (int i=0; i<prop_cnt; i++) {
    print_prop(&(p[i]), prefix);
  }
}

int images_overlap(Image src_image, Image dst_image, enum direction direction) {
  int src_offx, src_offy, dst_offx, dst_offy, width, height;

  switch (direction) {
  case UP:
    src_offx = 0; src_offy = 0;
    dst_offx = 0; dst_offy = 1;
    width = src_image.width;
    height = src_image.height-1;
    break;
  case DOWN:
    src_offx = 0; src_offy = 1;
    dst_offx = 0; dst_offy = 0;
    width = src_image.width;
    height = src_image.height-1;
    break;
  case LEFT:
    src_offx = 0; src_offy = 0;
    dst_offx = 1; dst_offy = 0;
    width = src_image.width-1;
    height = src_image.height;
    break;
  case RIGHT:
    src_offx = 1; src_offy = 0;
    dst_offx = 0; dst_offy = 0;
    width = src_image.width-1;
    height = src_image.height;
    break;
  default:
    printf("error");
    exit(1);
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

void create_allowed_tiles(struct tile *tiles, int tile_cnt) {
  int directions[4] = {UP, DOWN, LEFT, RIGHT};

  for (int d_idx=0; d_idx<4; d_idx++) {
    enum direction d = directions[d_idx];

    for (int i=0; i<tile_cnt; i++) {

      struct tile *t = &( tiles[i] );
      t->allowed_tiles[d] = malloc(sizeof(*(tiles[i].allowed_tiles)) * tile_cnt);
      t->allowed_tile_cnt[d] = 0;
      assert(t->allowed_tiles[d]);

      for (int j=0; j<tile_cnt; j++) {
        /* if (i==j) { */
        /*   continue; */
        /* } */

        if (images_overlap(tiles[i].image, tiles[j].image, d)) {
          t->allowed_tiles[d][ t->allowed_tile_cnt[d] ] = j;
          t->allowed_tile_cnt[d]++;
        }
      }
    }
  }
}

void destroy_allowed_tiles(struct tile *tiles, int tile_cnt) {
  for (int i=0; i<tile_cnt; i++) {
    int directions[4] = {UP, DOWN, LEFT, RIGHT};
    for (int d_idx=0; d_idx<4; d_idx++) {
      free(tiles[i].allowed_tiles[directions[d_idx]]);
    }
  }
}

struct tile *create_tiles(const char *filename, int tile_width, int tile_height, int *tile_cnt) {
  Image image = LoadImage(filename);

  int tile_xcnt = image.width - tile_width + 1;
  int tile_ycnt = image.height - tile_height + 1;
  *tile_cnt = tile_xcnt * tile_ycnt;

  struct tile *tiles = malloc(sizeof(*tiles) * (*tile_cnt));
  assert(tiles != NULL);

  for (int y=0; y<tile_ycnt; y++) {
    for (int x=0; x<tile_xcnt; x++) {
      struct tile *tile = &( tiles[y*tile_ycnt + x] );
      tile->image = ImageFromImage(image, (Rectangle){x, y, tile_width, tile_height});
    }
  }

  create_allowed_tiles(tiles, *tile_cnt);

  return tiles;
}

void destroy_tiles(struct tile *tiles, int tile_cnt) {
  destroy_allowed_tiles(tiles, tile_cnt);
  for (int i=0; i<tile_cnt; i++) {
    UnloadImage(tiles[i].image);
  }
  free(tiles);
}

void export_tiles(const char *path, struct tile *tiles, int tile_cnt) {
  char filename[128];
  for (int i=0; i<tile_cnt; i++) {
    sprintf(filename, "%s/%d.png", path, i);
    ExportImage(tiles[i].image, filename);
  }
}

void collapse(struct system *s, int cell_idx) {
  double _01 = (double)rand() / RAND_MAX;
  int idx = (int)(_01 * s->cells[cell_idx].tile_cnt);
  s->cells[cell_idx].tiles[0] = s->cells[cell_idx].tiles[idx];
  s->cells[cell_idx].tile_cnt = 1;
}

void add_prop(struct system *s, int src_cell_idx, int dst_cell_idx, enum direction direction) {
  assert(src_cell_idx >= 0);
  assert(src_cell_idx < s->cell_cnt);
  assert(dst_cell_idx >= 0);
  assert(dst_cell_idx < s->cell_cnt);

  struct prop *p = &( s->props[s->prop_cnt] );
  (s->prop_cnt)++;;
  p->src_cell_idx = src_cell_idx;
  p->dst_cell_idx = dst_cell_idx;
  p->direction = direction;
}

// add prop to update cell above the cell_idx
void add_prop_up(struct system *s, int src_cell_idx) {
  if (src_cell_idx - s->width >= 0) {
    add_prop(s, src_cell_idx, src_cell_idx - s->width, UP);
  }
}

void add_prop_down(struct system *s, int src_cell_idx) {
  if (src_cell_idx + s->width < s->cell_cnt) {
    add_prop(s, src_cell_idx, src_cell_idx + s->width, DOWN);
  }
}

void add_prop_left(struct system *s, int src_cell_idx) {
  if (src_cell_idx % s->width != 0) {
    add_prop(s, src_cell_idx, src_cell_idx - 1, LEFT);
  }
}

void add_prop_right(struct system *s, int src_cell_idx) {
  if (src_cell_idx % s->width != s->width - 1) {
    add_prop(s, src_cell_idx, src_cell_idx + 1, RIGHT);
  }
}

int tiles_allowed(struct system *s, int src_tile_idx, int dst_tile_idx, enum direction d) {
  int *allowed_tiles = s->tiles[ src_tile_idx ].allowed_tiles[d];
  int allowed_tile_cnt = s->tiles[ src_tile_idx ].allowed_tile_cnt[d];
  for (int i=0; i<allowed_tile_cnt; i++) {
    if (dst_tile_idx == allowed_tiles[i]) {
      return 1;
    }
  }
  return 0;
}

// Does the cell enable tile in the direction?
int tile_enabled(struct system *s, int tile_idx, int cell_idx, enum direction d) {
  struct cell *cell = &( s->cells[cell_idx] );

  // Go through all tiles in the cell and check whether they allow tile_idx in the specified direction
  for (int i=0; i<cell->tile_cnt; i++) {
    int cell_tile_idx = cell->tiles[i];

    if (tiles_allowed(s, cell_tile_idx, tile_idx, d)) {
      return 1;
    }
  }
  return 0;
}

// Updates tiles in the destination cell to those that are allowed by the source cell
// and propagate updates
void propagate_prop(struct system *s, struct prop *p) {
  int new_cnt = 0;

  struct cell *dst_cell = &( s->cells[ p->dst_cell_idx ] );
  // Go through all destination tiles and check whether they are enabled by the source cell
  for (int i=0; i<dst_cell->tile_cnt; i++) {
    int possible_dst_tile_idx = dst_cell->tiles[i];

    // If a destination tile is enabled by the source cell, keep it
    if (tile_enabled(s, possible_dst_tile_idx, p->src_cell_idx, p->direction)) {
      dst_cell->tiles[new_cnt] = possible_dst_tile_idx;
      new_cnt++;
    }
  }

  assert(new_cnt);

  if (dst_cell->tile_cnt != new_cnt) {
    if (p->direction != DOWN) add_prop_up(s, p->dst_cell_idx);
    if (p->direction != UP) add_prop_down(s, p->dst_cell_idx);
    if (p->direction != RIGHT) add_prop_left(s, p->dst_cell_idx);
    if (p->direction != LEFT) add_prop_right(s, p->dst_cell_idx);
  }

  dst_cell->tile_cnt = new_cnt;
}

void propagate(struct system *s, int cell_idx) {
  s->prop_cnt = 0;

  add_prop_up(s, cell_idx);
  add_prop_down(s, cell_idx);
  add_prop_left(s, cell_idx);
  add_prop_right(s, cell_idx);

  for (int i=0; i<s->prop_cnt; i++) {
    struct prop *p = &( s->props[i] );
    propagate_prop(s, p);
  }
}

int next_cell(struct system *s) {
  int min_idx = -1;
  int min_tile_cnt = INT_MAX;

  for (int i=0; i<s->cell_cnt; i++) {
    if (s->cells[i].tile_cnt != 1 && s->cells[i].tile_cnt < min_tile_cnt) {
      min_tile_cnt = s->cells[i].tile_cnt;
      min_idx = i;
    }
  }

  return min_idx;
}

void run(struct system *s) {
  int cnt = 0;
  while (1) {
    int cell_idx = next_cell(s);
    if (cell_idx == -1 || cnt == 1000) {
      break;
    }
    //int cell_idx = s->cell_cnt / 2 + s->width / 2;
    collapse(s, cell_idx);
    propagate(s, cell_idx);
    cnt++;
  }
  printf("%d iterations\n", cnt);
}

struct system *create_system(int width, int height, const char *tile_filename, int tile_width, int tile_height) {
  struct system *s = malloc(sizeof(*s));
  assert(s!=NULL);

  s->width = width;
  s->height = height;
  s->cell_cnt = width * height;
  s->tile_width = tile_width;
  s->tile_height = tile_height;

  s->tiles = create_tiles(tile_filename, tile_width, tile_height, &(s->tile_cnt));

  s->cells = malloc(sizeof(*(s->cells)) * s->cell_cnt);
  assert(s->cells!=NULL);
  for (int i=0; i<s->cell_cnt; i++) {
    s->cells[i].tiles = malloc(sizeof(*(s->cells[i].tiles)) * s->cell_cnt);
    assert(s->cells[i].tiles);
  }

  s->props = malloc(sizeof(*(s->props)) * s->cell_cnt);
  assert(s->props!=NULL);
  s->prop_cnt = 0;

  return s;
}

void destroy_system(struct system *s) {
  free(s->props);
  for (int i=0; i<s->cell_cnt; i++) {
    free(s->cells[i].tiles);
  }
  free(s->cells);
  destroy_tiles(s->tiles, s->tile_cnt);
  free(s);
}

void init_system(struct system *s) {
  for (int i=0; i<s->cell_cnt; i++) {
    s->cells[i].tile_cnt = s->tile_cnt;
    for (int j=0; j<s->tile_cnt; j++) {
      s->cells[i].tiles[j] = j;
    }
  }
  s->prop_cnt = 0;
}

void export(struct system *s, const char *filename) {
  Image output = GenImageColor(s->width, s->height, RAYWHITE);
  for (int y=0; y<s->height; y++) {
    for (int x=0; x<s->width; x++) {
      struct cell *cell = &( s->cells[y * s->width + x] );
      double r=0, g=0, b=0, a=0;
      for (int i=0; i<cell->tile_cnt; i++) {
        struct tile *tile = &( s->tiles[ cell->tiles[i] ] );
        Color *colors = GetImageData(tile->image);
        r += colors[0].r;
        g += colors[0].g;
        b += colors[0].b;
        a += colors[0].a;
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

void export_cnts(struct system *s, const char *filename) {
  Image output = GenImageColor(s->width, s->height, RAYWHITE);
  for (int y=0; y<s->height; y++) {
    for (int x=0; x<s->width; x++) {
      struct cell *cell = &( s->cells[y * s->width + x] );
      int c = ((double)cell->tile_cnt / s->tile_cnt) * 255;
      ImageDrawPixel(&output, x, y, (Color){c, c, c, 255});
    }
  }

  ExportImage(output, filename);
}

#endif
