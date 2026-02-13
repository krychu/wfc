// wfc tests
//
// Run via: make test

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define WFC_IMPLEMENTATION
#define WFC_USE_STB
#include "wfc.h"

////////////////////////////////////////////////////////////////////////////////
// Test framework
////////////////////////////////////////////////////////////////////////////////

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) \
  do { if (!(cond)) { printf("  FAIL: %s\n", msg); return 0; } } while (0)

#define ASSERT_EQ(a, b, msg) \
  do { if ((a) != (b)) { printf("  FAIL: %s (got %d, expected %d)\n", msg, (int)(a), (int)(b)); return 0; } } while (0)

#define RUN_TEST(fn) \
  do { \
    tests_run++; \
    printf("  %-50s", #fn); \
    fflush(stdout); \
    if (fn()) { tests_passed++; printf("PASS\n"); } \
    else { tests_failed++; printf("FAIL\n"); } \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
// Helpers
////////////////////////////////////////////////////////////////////////////////

static struct wfc_image *make_image(int w, int h, int comp, const unsigned char *pixels)
{
  struct wfc_image *img = wfc_img_create(w, h, comp);
  if (img && pixels)
    memcpy(img->data, pixels, w * h * comp);
  return img;
}

static unsigned char pixel_at(struct wfc_image *img, int x, int y, int c)
{
  return img->data[(y * img->width + x) * img->component_cnt + c];
}

#define FIXED_SEED 2

////////////////////////////////////////////////////////////////////////////////
// Unit tests
////////////////////////////////////////////////////////////////////////////////

static int test_img_create_destroy(void)
{
  struct wfc_image *img = wfc_img_create(10, 20, 4);
  ASSERT(img != NULL, "wfc_img_create returned NULL");
  ASSERT_EQ(img->width, 10, "width");
  ASSERT_EQ(img->height, 20, "height");
  ASSERT_EQ(img->component_cnt, 4, "component_cnt");
  wfc_img_destroy(img);

  // Destroy NULL should not crash
  wfc_img_destroy(NULL);
  return 1;
}

static int test_img_flip_horizontally(void)
{
  // 3x1 image, 3 components: [R, G, B] -> expect [B, G, R]
  unsigned char pixels[] = {
    255, 0, 0,    0, 255, 0,    0, 0, 255
  };
  struct wfc_image *img = make_image(3, 1, 3, pixels);
  ASSERT(img != NULL, "make_image failed");

  struct wfc_image *flipped = wfc__img_flip_horizontally(img);
  ASSERT(flipped != NULL, "flip returned NULL");

  ASSERT_EQ(pixel_at(flipped, 0, 0, 0), 0, "flipped[0].r");
  ASSERT_EQ(pixel_at(flipped, 0, 0, 1), 0, "flipped[0].g");
  ASSERT_EQ(pixel_at(flipped, 0, 0, 2), 255, "flipped[0].b");

  ASSERT_EQ(pixel_at(flipped, 1, 0, 0), 0, "flipped[1].r");
  ASSERT_EQ(pixel_at(flipped, 1, 0, 1), 255, "flipped[1].g");
  ASSERT_EQ(pixel_at(flipped, 1, 0, 2), 0, "flipped[1].b");

  ASSERT_EQ(pixel_at(flipped, 2, 0, 0), 255, "flipped[2].r");
  ASSERT_EQ(pixel_at(flipped, 2, 0, 1), 0, "flipped[2].g");
  ASSERT_EQ(pixel_at(flipped, 2, 0, 2), 0, "flipped[2].b");

  wfc_img_destroy(img);
  wfc_img_destroy(flipped);
  return 1;
}

static int test_img_flip_vertically(void)
{
  unsigned char pixels[] = { 10, 20, 30 };
  struct wfc_image *img = make_image(1, 3, 1, pixels);
  ASSERT(img != NULL, "make_image failed");

  struct wfc_image *flipped = wfc__img_flip_vertically(img);
  ASSERT(flipped != NULL, "flip returned NULL");

  ASSERT_EQ(pixel_at(flipped, 0, 0, 0), 30, "top row");
  ASSERT_EQ(pixel_at(flipped, 0, 1, 0), 20, "middle row");
  ASSERT_EQ(pixel_at(flipped, 0, 2, 0), 10, "bottom row");

  wfc_img_destroy(img);
  wfc_img_destroy(flipped);
  return 1;
}

static int test_img_rotate90(void)
{
  unsigned char pixels[] = { 1, 2, 3, 4 };
  struct wfc_image *img = make_image(2, 2, 1, pixels);
  ASSERT(img != NULL, "make_image failed");

  struct wfc_image *r1 = wfc__img_rotate90(img, 1);
  ASSERT(r1 != NULL, "rotate90(1) returned NULL");
  ASSERT_EQ(pixel_at(r1, 0, 0, 0), 3, "r90 [0,0]");
  ASSERT_EQ(pixel_at(r1, 1, 0, 0), 1, "r90 [1,0]");
  ASSERT_EQ(pixel_at(r1, 0, 1, 0), 4, "r90 [0,1]");
  ASSERT_EQ(pixel_at(r1, 1, 1, 0), 2, "r90 [1,1]");

  struct wfc_image *r2 = wfc__img_rotate90(img, 2);
  ASSERT(r2 != NULL, "rotate90(2) returned NULL");
  ASSERT_EQ(pixel_at(r2, 0, 0, 0), 4, "r180 [0,0]");
  ASSERT_EQ(pixel_at(r2, 1, 0, 0), 3, "r180 [1,0]");
  ASSERT_EQ(pixel_at(r2, 0, 1, 0), 2, "r180 [0,1]");
  ASSERT_EQ(pixel_at(r2, 1, 1, 0), 1, "r180 [1,1]");

  struct wfc_image *r3 = wfc__img_rotate90(img, 3);
  ASSERT(r3 != NULL, "rotate90(3) returned NULL");
  ASSERT_EQ(pixel_at(r3, 0, 0, 0), 2, "r270 [0,0]");
  ASSERT_EQ(pixel_at(r3, 1, 0, 0), 4, "r270 [1,0]");
  ASSERT_EQ(pixel_at(r3, 0, 1, 0), 1, "r270 [0,1]");
  ASSERT_EQ(pixel_at(r3, 1, 1, 0), 3, "r270 [1,1]");

  wfc_img_destroy(img);
  wfc_img_destroy(r1);
  wfc_img_destroy(r2);
  wfc_img_destroy(r3);
  return 1;
}

static int test_img_cmp(void)
{
  unsigned char a[] = { 1, 2, 3, 4 };
  unsigned char b[] = { 1, 2, 3, 4 };
  unsigned char c[] = { 1, 2, 3, 5 };

  struct wfc_image *ia = make_image(2, 2, 1, a);
  struct wfc_image *ib = make_image(2, 2, 1, b);
  struct wfc_image *ic = make_image(2, 2, 1, c);
  ASSERT(ia && ib && ic, "make_image failed");

  ASSERT(wfc__img_cmp(ia, ib) == 1, "identical images should be equal");
  ASSERT(wfc__img_cmp(ia, ic) == 0, "different images should not be equal");

  struct wfc_image *id = make_image(1, 4, 1, a);
  ASSERT(wfc__img_cmp(ia, id) == 0, "different dimensions should not be equal");

  wfc_img_destroy(ia);
  wfc_img_destroy(ib);
  wfc_img_destroy(ic);
  wfc_img_destroy(id);
  return 1;
}

static int test_img_cmpoverlap(void)
{
  unsigned char ta[] = { 1,2,3, 4,5,6, 7,8,9 };
  unsigned char tb[] = { 2,3,10, 5,6,11, 8,9,12 };

  struct wfc_image *a = make_image(3, 3, 1, ta);
  struct wfc_image *b = make_image(3, 3, 1, tb);
  ASSERT(a && b, "make_image failed");

  ASSERT(wfc__img_cmpoverlap(a, b, WFC_RIGHT) == 1,
         "A right-overlap B should match");
  ASSERT(wfc__img_cmpoverlap(b, a, WFC_RIGHT) == 0,
         "B right-overlap A should not match");
  ASSERT(wfc__img_cmpoverlap(b, a, WFC_LEFT) == 1,
         "B left-overlap A should match");

  unsigned char tc[] = { 1,2,3, 4,5,6, 7,8,9 };
  unsigned char td[] = { 4,5,6, 7,8,9, 10,11,12 };

  struct wfc_image *c = make_image(3, 3, 1, tc);
  struct wfc_image *d = make_image(3, 3, 1, td);
  ASSERT(c && d, "make_image failed");

  ASSERT(wfc__img_cmpoverlap(c, d, WFC_DOWN) == 1,
         "C down-overlap D should match");
  ASSERT(wfc__img_cmpoverlap(d, c, WFC_UP) == 1,
         "D up-overlap C should match");
  ASSERT(wfc__img_cmpoverlap(c, d, WFC_UP) == 0,
         "C up-overlap D should not match");

  wfc_img_destroy(a);
  wfc_img_destroy(b);
  wfc_img_destroy(c);
  wfc_img_destroy(d);
  return 1;
}

static int test_img_expand(void)
{
  unsigned char pixels[] = { 1, 2, 3, 4 };
  struct wfc_image *img = make_image(2, 2, 1, pixels);
  ASSERT(img != NULL, "make_image failed");

  struct wfc_image *exp = wfc__img_expand(img, 1, 1);
  ASSERT(exp != NULL, "expand returned NULL");
  ASSERT_EQ(exp->width, 3, "expanded width");
  ASSERT_EQ(exp->height, 3, "expanded height");

  ASSERT_EQ(pixel_at(exp, 0, 0, 0), 1, "exp [0,0]");
  ASSERT_EQ(pixel_at(exp, 1, 0, 0), 2, "exp [1,0]");
  ASSERT_EQ(pixel_at(exp, 2, 0, 0), 1, "exp [2,0]");
  ASSERT_EQ(pixel_at(exp, 0, 1, 0), 3, "exp [0,1]");
  ASSERT_EQ(pixel_at(exp, 1, 1, 0), 4, "exp [1,1]");
  ASSERT_EQ(pixel_at(exp, 2, 1, 0), 3, "exp [2,1]");
  ASSERT_EQ(pixel_at(exp, 0, 2, 0), 1, "exp [0,2]");
  ASSERT_EQ(pixel_at(exp, 1, 2, 0), 2, "exp [1,2]");
  ASSERT_EQ(pixel_at(exp, 2, 2, 0), 1, "exp [2,2]");

  wfc_img_destroy(img);
  wfc_img_destroy(exp);
  return 1;
}

static int test_create_tile_image(void)
{
  unsigned char pixels[] = {
     1,  2,  3,  4,
     5,  6,  7,  8,
     9, 10, 11, 12,
    13, 14, 15, 16
  };
  struct wfc_image *img = make_image(4, 4, 1, pixels);
  ASSERT(img != NULL, "make_image failed");

  struct wfc_image *tile = wfc__create_tile_image(img, 1, 1, 2, 2);
  ASSERT(tile != NULL, "create_tile_image returned NULL");
  ASSERT_EQ(tile->width, 2, "tile width");
  ASSERT_EQ(tile->height, 2, "tile height");
  ASSERT_EQ(pixel_at(tile, 0, 0, 0), 6, "tile [0,0]");
  ASSERT_EQ(pixel_at(tile, 1, 0, 0), 7, "tile [1,0]");
  ASSERT_EQ(pixel_at(tile, 0, 1, 0), 10, "tile [0,1]");
  ASSERT_EQ(pixel_at(tile, 1, 1, 0), 11, "tile [1,1]");

  wfc_img_destroy(img);
  wfc_img_destroy(tile);
  return 1;
}

static int test_remove_duplicate_tiles(void)
{
  unsigned char pa[] = { 1, 2, 3, 4 };
  unsigned char pb[] = { 5, 6, 7, 8 };

  int tile_cnt = 4;
  struct wfc__tile *tiles = malloc(sizeof(*tiles) * tile_cnt);
  ASSERT(tiles != NULL, "malloc failed");

  tiles[0].freq = 1;
  tiles[0].image = make_image(2, 2, 1, pa);
  tiles[1].freq = 1;
  tiles[1].image = make_image(2, 2, 1, pb);
  tiles[2].freq = 1;
  tiles[2].image = make_image(2, 2, 1, pa);
  tiles[3].freq = 1;
  tiles[3].image = make_image(2, 2, 1, pb);

  int result = wfc__remove_duplicate_tiles(&tiles, &tile_cnt);
  ASSERT(result != 0, "remove_duplicate_tiles failed");
  ASSERT_EQ(tile_cnt, 2, "unique tile count");
  ASSERT_EQ(tiles[0].freq, 2, "tile A frequency");
  ASSERT_EQ(tiles[1].freq, 2, "tile B frequency");
  ASSERT_EQ(tiles[0].image->data[0], 1, "tile A first pixel");
  ASSERT_EQ(tiles[1].image->data[0], 5, "tile B first pixel");

  for (int i = 0; i < tile_cnt; i++)
    wfc_img_destroy(tiles[i].image);
  free(tiles);
  return 1;
}

static int test_img_copy(void)
{
  unsigned char pixels[] = { 10, 20, 30, 40 };
  struct wfc_image *img = make_image(2, 2, 1, pixels);
  ASSERT(img != NULL, "make_image failed");

  struct wfc_image *copy = wfc_img_copy(img);
  ASSERT(copy != NULL, "wfc_img_copy returned NULL");
  ASSERT(wfc__img_cmp(img, copy) == 1, "copy should equal original");

  copy->data[0] = 99;
  ASSERT(wfc__img_cmp(img, copy) == 0, "modified copy should differ");
  ASSERT_EQ(img->data[0], 10, "original should be unchanged");

  wfc_img_destroy(img);
  wfc_img_destroy(copy);
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Integration tests (require sample images in tests/fixtures/)
////////////////////////////////////////////////////////////////////////////////

static const char *sample_files[] = {
  "tests/fixtures/cave.png",
  "tests/fixtures/wrinkles.png",
  "tests/fixtures/sand.png",
  "tests/fixtures/curl.png",
  "tests/fixtures/twolines.png",
  "tests/fixtures/twolines2.png",
  "tests/fixtures/square.png",
  NULL
};

static struct wfc *run_wfc_with_seed(struct wfc_image *input, unsigned int seed)
{
  struct wfc *wfc = wfc_overlapping(
    64, 64, input, 3, 3, 1, 1, 1, 1
  );
  if (!wfc) return NULL;

  wfc->seed = seed;
  wfc__seed_rng(wfc->rng, wfc->seed);
  wfc->collapsed_cell_cnt = 0;
  wfc__init_cells(wfc);

  if (!wfc_run(wfc, -1)) {
    wfc_destroy(wfc);
    return NULL;
  }
  return wfc;
}

static int test_deterministic(void)
{
  struct wfc_image *input = wfc_img_load("tests/fixtures/cave.png");
  ASSERT(input != NULL, "failed to load cave.png");

  struct wfc *wfc1 = run_wfc_with_seed(input, FIXED_SEED);
  ASSERT(wfc1 != NULL, "first WFC run failed (contradiction)");
  struct wfc_image *out1 = wfc_output_image(wfc1);
  ASSERT(out1 != NULL, "output_image 1 failed");

  struct wfc *wfc2 = run_wfc_with_seed(input, FIXED_SEED);
  ASSERT(wfc2 != NULL, "second WFC run failed (contradiction)");
  struct wfc_image *out2 = wfc_output_image(wfc2);
  ASSERT(out2 != NULL, "output_image 2 failed");

  ASSERT_EQ(out1->width, out2->width, "output widths differ");
  ASSERT_EQ(out1->height, out2->height, "output heights differ");

  int size = out1->width * out1->height * out1->component_cnt;
  ASSERT(memcmp(out1->data, out2->data, size) == 0,
         "outputs with same seed should be identical");

  wfc_img_destroy(out1);
  wfc_img_destroy(out2);
  wfc_destroy(wfc1);
  wfc_destroy(wfc2);
  wfc_img_destroy(input);
  return 1;
}

static int file_exists(const char *path)
{
  FILE *f = fopen(path, "rb");
  if (f) { fclose(f); return 1; }
  return 0;
}

static int test_regression(void)
{
  int all_ok = 1;
  int generated = 0;

  for (int i = 0; sample_files[i]; i++) {
    const char *basename = strrchr(sample_files[i], '/');
    basename = basename ? basename + 1 : sample_files[i];

    char refpath[256];
    snprintf(refpath, sizeof(refpath), "tests/reference/%s", basename);

    char outpath[256];
    snprintf(outpath, sizeof(outpath), "tests/output/%s", basename);

    struct wfc_image *input = wfc_img_load(sample_files[i]);
    if (!input) {
      printf("\n    WARNING: cannot load %s, skipping\n", sample_files[i]);
      continue;
    }

    struct wfc *wfc = run_wfc_with_seed(input, FIXED_SEED);
    if (!wfc) {
      printf("\n    FAIL: %s - WFC contradiction\n", sample_files[i]);
      wfc_img_destroy(input);
      all_ok = 0;
      continue;
    }

    struct wfc_image *output = wfc_output_image(wfc);
    if (!output) {
      printf("\n    FAIL: %s - output_image failed\n", sample_files[i]);
      wfc_destroy(wfc);
      wfc_img_destroy(input);
      all_ok = 0;
      continue;
    }

    wfc_img_save(output, outpath);

    if (!file_exists(refpath)) {
      wfc_img_save(output, refpath);
      printf("\n    GENERATED reference: %s", refpath);
      generated = 1;
    } else {
      struct wfc_image *ref = wfc_img_load(refpath);
      if (!ref) {
        printf("\n    FAIL: cannot load reference %s\n", refpath);
        all_ok = 0;
      } else {
        if (!wfc__img_cmp(output, ref)) {
          printf("\n    FAIL: %s differs from reference\n", basename);
          all_ok = 0;
        }
        wfc_img_destroy(ref);
      }
    }

    wfc_img_destroy(output);
    wfc_destroy(wfc);
    wfc_img_destroy(input);
  }

  if (generated)
    printf("\n    (Reference images generated on first run. Re-run to verify.)\n");

  ASSERT(all_ok, "regression check failed");
  return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
  int run_unit = 1;
  int run_integration = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--unit") == 0) { run_unit = 1; run_integration = 0; }
    else if (strcmp(argv[i], "--integration") == 0) { run_unit = 0; run_integration = 1; }
  }

  printf("wfc tests\n");
  printf("=========\n\n");

  if (run_unit) {
    printf("Unit tests:\n");
    RUN_TEST(test_img_create_destroy);
    RUN_TEST(test_img_copy);
    RUN_TEST(test_img_flip_horizontally);
    RUN_TEST(test_img_flip_vertically);
    RUN_TEST(test_img_rotate90);
    RUN_TEST(test_img_cmp);
    RUN_TEST(test_img_cmpoverlap);
    RUN_TEST(test_img_expand);
    RUN_TEST(test_create_tile_image);
    RUN_TEST(test_remove_duplicate_tiles);
    printf("\n");
  }

  if (run_integration) {
    struct wfc_image *probe = wfc_img_load("tests/fixtures/cave.png");
    if (probe) {
      wfc_img_destroy(probe);
      printf("Integration tests:\n");
      RUN_TEST(test_deterministic);
      RUN_TEST(test_regression);
      printf("\n");
    } else {
      printf("Integration tests: SKIPPED (fixture images not found)\n\n");
    }
  }

  printf("Results: %d/%d passed", tests_passed, tests_run);
  if (tests_failed > 0)
    printf(", %d FAILED", tests_failed);
  printf("\n");

  return tests_failed > 0 ? 1 : 0;
}
