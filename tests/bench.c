// WFC Benchmarks
//
// Run via: make bench

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#define WFC_IMPLEMENTATION
#define WFC_USE_STB
#include "wfc.h"

#define FIXED_SEED 5
#define BENCH_RUNS 5
#define BENCH_OUTPUT_SIZE 128

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

static double timespec_ms(struct timespec *start, struct timespec *end)
{
  return (end->tv_sec - start->tv_sec) * 1000.0 +
         (end->tv_nsec - start->tv_nsec) / 1000000.0;
}

static int cmp_double(const void *a, const void *b)
{
  double da = *(const double *)a;
  double db = *(const double *)b;
  if (da < db) return -1;
  if (da > db) return 1;
  return 0;
}

int main(void)
{
  printf("WFC Benchmarks (%d runs, median, %dx%d output)\n",
         BENCH_RUNS, BENCH_OUTPUT_SIZE, BENCH_OUTPUT_SIZE);
  printf("\n");
  printf("tiles    - unique tiles after extraction, flips, rotations, and dedup\n");
  printf("setup_ms - tile extraction, dedup, and rule computation (wfc_overlapping)\n");
  printf("solve_ms - constraint propagation and collapse (wfc_run)\n");
  printf("total_ms - end-to-end (setup + solve)\n");
  printf("\n");
  printf("%-20s %6s %10s %10s %10s\n", "sample", "tiles", "setup_ms", "solve_ms", "total_ms");
  printf("%-20s %6s %10s %10s %10s\n", "------", "-----", "--------", "--------", "--------");

  for (int i = 0; sample_files[i]; i++) {
    struct wfc_image *input = wfc_img_load(sample_files[i]);
    if (!input) {
      printf("%-20s  (cannot load)\n", sample_files[i]);
      continue;
    }

    const char *basename = strrchr(sample_files[i], '/');
    basename = basename ? basename + 1 : sample_files[i];

    double setup_times[BENCH_RUNS];
    double solve_times[BENCH_RUNS];
    double total_times[BENCH_RUNS];
    int tile_count = 0;

    for (int r = 0; r < BENCH_RUNS; r++) {
      struct timespec t0, t1, t2;

      clock_gettime(CLOCK_MONOTONIC, &t0);

      struct wfc *wfc = wfc_overlapping(
        BENCH_OUTPUT_SIZE, BENCH_OUTPUT_SIZE,
        input, 3, 3, 1, 1, 1, 1
      );
      if (!wfc) {
        setup_times[r] = solve_times[r] = total_times[r] = -1;
        continue;
      }

      wfc->seed = FIXED_SEED;
      srand(wfc->seed);
      wfc->collapsed_cell_cnt = 0;
      wfc__init_cells(wfc);

      clock_gettime(CLOCK_MONOTONIC, &t1);

      wfc_run(wfc, -1);

      clock_gettime(CLOCK_MONOTONIC, &t2);

      tile_count = wfc->tile_cnt;
      setup_times[r] = timespec_ms(&t0, &t1);
      solve_times[r] = timespec_ms(&t1, &t2);
      total_times[r] = timespec_ms(&t0, &t2);

      wfc_destroy(wfc);
    }

    qsort(setup_times, BENCH_RUNS, sizeof(double), cmp_double);
    qsort(solve_times, BENCH_RUNS, sizeof(double), cmp_double);
    qsort(total_times, BENCH_RUNS, sizeof(double), cmp_double);

    int mid = BENCH_RUNS / 2;
    printf("%-20s %6d %10.1f %10.1f %10.1f\n",
           basename, tile_count,
           setup_times[mid], solve_times[mid], total_times[mid]);

    wfc_img_destroy(input);
  }

  printf("\n");
  return 0;
}
