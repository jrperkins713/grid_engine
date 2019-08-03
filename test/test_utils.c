// Copyright (c) 2019 Tim Perkins

// Licensed under an MIT style license, see LICENSE.md for details.
// You are free to copy and modify this code. Happy hacking!

#include "grid_engine/grid_engine.h"

void conway_loop_func(ge_grid_t* restrict grid, void* restrict user_data, uint32_t time_ms)
{
  // Do nothing
  (void) grid;
  (void) user_data;
  (void) time_ms;

  if (time_ms > 3000) {
    ge_grid_set_coord_wrapped(grid, (ge_coord_t){10, 80}, 200);
    ge_grid_set_coord_wrapped(grid, (ge_coord_t){50, 50}, 200);
    ge_grid_set_coord_wrapped(grid, (ge_coord_t){150, 10}, 200);
  }
}

int main(void)
{
  const size_t width = 100;
  const size_t height = 100;
  ge_grid_t* grid = ge_grid_create(width, height);
  // Make a glider pattern
  ge_coord_t coords[300];
  size_t num_coords = 0;
  num_coords = ge_utils_line_coords((ge_coord_t){0, 10}, (ge_coord_t){10, 80}, coords, 100);
  for (size_t ii = 0; ii < num_coords; ++ii) {
    GE_LOG_INFO("Setting coord #%d: (%li, %li)", ii, coords[ii].x, coords[ii].y);
    ge_grid_set_coord_wrapped(grid, coords[ii], 255);
  }

  num_coords = ge_utils_line_coords((ge_coord_t){10, 10}, (ge_coord_t){50, 50}, coords, 100);
  for (size_t ii = 0; ii < num_coords; ++ii) {
    GE_LOG_INFO("Setting coord #%d: (%li, %li)", ii, coords[ii].x, coords[ii].y);
    ge_grid_set_coord_wrapped(grid, coords[ii], 255);
  }

  num_coords = ge_utils_line_coords((ge_coord_t){10, 0}, (ge_coord_t){150, 10}, coords, 300);
  for (size_t ii = 0; ii < num_coords; ++ii) {
    GE_LOG_INFO("Setting coord #%d: (%li, %li)", ii, coords[ii].x, coords[ii].y);
    ge_grid_set_coord_wrapped(grid, coords[ii], 255);
  }

  /* ge_grid_set_coord_wrapped(grid, (ge_coord_t){10, 80}, 200); */
  /* ge_grid_set_coord_wrapped(grid, (ge_coord_t){50, 50}, 200); */
  /* ge_grid_set_coord_wrapped(grid, (ge_coord_t){80, 10}, 200); */

  // The EZ loop data
  ez_loop_data_t ez_loop_data = {
      .grid = grid,
      .user_data = NULL,
      .loop_func = conway_loop_func,
      .event_func = NULL,
  };
  // RUN THE LOOP!
  return ge_ez_loop(&ez_loop_data);
}
