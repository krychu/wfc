#include <stdio.h>
#include <assert.h>
#include "wfc.c"

int main(void) {
  struct system *s = create_system(128, 128, "tiles/ytsquare.png", 3, 3);

  assert(s->tiles[0].allowed_tile_cnt[RIGHT] == 1);
  assert(s->tiles[0].allowed_tiles[RIGHT][0] == 1);

  assert(s->tiles[0].allowed_tile_cnt[LEFT] == 7);
  assert(s->tiles[0].allowed_tiles[LEFT][0] == 6);
  assert(s->tiles[0].allowed_tiles[LEFT][1] == 13);
  assert(s->tiles[0].allowed_tiles[LEFT][2] == 20);
  assert(s->tiles[0].allowed_tiles[LEFT][3] == 27);
  assert(s->tiles[0].allowed_tiles[LEFT][4] == 34);
  assert(s->tiles[0].allowed_tiles[LEFT][5] == 41);
  assert(s->tiles[0].allowed_tiles[LEFT][6] == 48);

  assert(s->tiles[0].allowed_tile_cnt[UP] == 7);
  assert(s->tiles[0].allowed_tiles[UP][0] == 42);
  assert(s->tiles[0].allowed_tiles[UP][1] == 43);
  assert(s->tiles[0].allowed_tiles[UP][2] == 44);
  assert(s->tiles[0].allowed_tiles[UP][3] == 45);
  assert(s->tiles[0].allowed_tiles[UP][4] == 46);
  assert(s->tiles[0].allowed_tiles[UP][5] == 47);
  assert(s->tiles[0].allowed_tiles[UP][6] == 48);

  assert(s->tiles[0].allowed_tile_cnt[DOWN] == 1);
  assert(s->tiles[0].allowed_tiles[DOWN][0] == 7);



  assert(s->tiles[30].allowed_tile_cnt[RIGHT] == 2);
  assert(s->tiles[30].allowed_tiles[RIGHT][0] == 31);
  assert(s->tiles[30].allowed_tiles[RIGHT][1] == 32);

  assert(s->tiles[30].allowed_tile_cnt[UP] == 2);


  assert(s->tiles[1].allowed_tile_cnt[RIGHT] == 4);
  assert(s->tiles[1].allowed_tiles[RIGHT][0] == 2);
  assert(s->tiles[1].allowed_tiles[RIGHT][1] == 3);
  assert(s->tiles[1].allowed_tiles[RIGHT][2] == 4);
  assert(s->tiles[1].allowed_tiles[RIGHT][3] == 5);
}
