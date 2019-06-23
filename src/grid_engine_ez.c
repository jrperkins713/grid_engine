// Copyright (c) 2019 Tim Perkins

// Licensed under an MIT style license, see LICENSE.md for details.
// You are free to copy and modify this code. Happy hacking!

#include "grid_engine/grid_engine_ez.h"

#include <stdio.h>

// Target 60 frames per second: 1000 ms / 60 loops = 17 ms / loop
static const uint32_t TARGET_LOOP_MS = 17;

int ge_ez_loop(const size_t width, const size_t height, uint8_t* const restrict pixel_arr,
               void* const user_data, const ge_ez_loop_func_t loop_func)
{
  ge_init();

  ge_grid_t* grid = ge_grid_new(width, height, pixel_arr);

  if (ge_grid_window_create(grid) != GE_OK) {
    fprintf(stderr, "Cannot create window\n");
    return 1;
  }

  while (!ge_should_quit()) {
    const uint32_t loop_start_ms = ge_get_time_ms();
    ge_event_t event;
    while (ge_poll_events(&event)) {
      // TODO Handle events, once we have those
    }
    loop_func(width, height, pixel_arr, user_data, loop_start_ms);
    if (ge_grid_window_redraw(grid) != GE_OK) {
      fprintf(stderr, "Cannot draw window\n");
      return 1;
    }
    const uint32_t loop_end_ms = ge_get_time_ms();
    const uint32_t delta_time_ms = (loop_end_ms > loop_start_ms ? loop_end_ms - loop_start_ms : 0);
    const uint32_t delay_ms = (TARGET_LOOP_MS > delta_time_ms ? TARGET_LOOP_MS - delta_time_ms : 0);
    ge_sleep_ms(delay_ms);
  }

  if (ge_grid_window_destroy(grid) != GE_OK) {
    fprintf(stderr, "Cannot destroy window\n");
    return 1;
  }

  ge_grid_free(grid);

  ge_quit();

  return 0;
}