#include "wfc.c"

int main2(void) {
  const int screen_width = 800;
  const int screen_height = 450;

  InitWindow(screen_width, screen_height, "krychu");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();

    ClearBackground(RAYWHITE);
    DrawText("Hello World", 190, 200, 20, LIGHTGRAY);
    EndDrawing();
  }

  CloseWindow();

  return 0;
}

int main3(void) {
  int tile_cnt;
  struct tile *tiles = create_tiles("tiles/ytsquare.png", 3, 3, &tile_cnt);
  export_tiles("./tmp", tiles, tile_cnt);
  destroy_tiles(tiles, tile_cnt);

  return 0;
}

int main(void) {
  struct system *s = create_system(36, 36, "tiles/ytsquare.png", 3, 3);
  //struct system *s = create_system(36, 36, "tiles/square2.png", 3, 3);
  init_system(s);
  run(s);
  //printf("%d\n", s->cells[2].tile_cnt);
  export(s, "tmp/output.png");
  export_cnts(s, "tmp/output_cnts.png");
  destroy_system(s);

  return 0;
}
