// Copyright (c) 2019 Tim Perkins

// Licensed under an MIT style license, see LICENSE.md for details.
// You are free to copy and modify this code. Happy hacking!

#include "grid_engine/grid_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*
 * It's assumed that the ball never moves more than one pixel per update. (This effectively caps the
 * maximum velocity at about 60 pixels per second.) Lower velocities are achieved by only moving
 * once every N updates. The heading of the ball is defined as a sequence of three pixel updates.
 * This allows for 24 possible headings. (See the `BOUNCE_` tables below for details.)
 *
 * When the ball hits the paddle and bounces East or West (but not North or South), the ball can be
 * hit upwards or downwards depending on were the ball contacted the paddle. This can change the
 * heading of the bounce. (See the `HIT_` tables below for details.) This is implemented by making
 * the paddle have special valued pixels, which are used to lookup how the hit should be modified.
 * (See `VALUE_PADDLE_HIT_` for the special values.)
 */

// The reset time before the ball launches
const uint32_t RESET_TIME_MS = 3000;

// Anything at or above VALUE_ON is considered an obstacle
const uint8_t VALUE_ON = 150;
const uint8_t VALUE_OFF = 0;

// These are used to fade the grid each loop
const uint8_t VALUE_FADE = 100;
const uint8_t VALUE_FADE_DELTA = 10;

typedef enum value_paddle_hit {
  VALUE_PADDLE_HIT_UU = 255,  // Up x2
  VALUE_PADDLE_HIT_U = 254,   // Up
  VALUE_PADDLE_HIT_N = 253,   // Neutral
  VALUE_PADDLE_HIT_D = 252,   // Down
  VALUE_PADDLE_HIT_DD = 251,  // Down x2
} value_paddle_hit_t;

typedef enum heading {
  HEADING_NONE = -1,
  // North quadrant
  HEADING_N = 0,
  HEADING_N3E,
  HEADING_NNE,
  HEADING_NE,
  HEADING_NEE,
  HEADING_NE3,
  // East quadrant
  HEADING_E,
  HEADING_SE3,
  HEADING_SEE,
  HEADING_SE,
  HEADING_SSE,
  HEADING_S3E,
  // South quadrant
  HEADING_S,
  HEADING_S3W,
  HEADING_SSW,
  HEADING_SW,
  HEADING_SWW,
  HEADING_SW3,
  // West quadrant
  HEADING_W,
  HEADING_NW3,
  HEADING_NWW,
  HEADING_NW,
  HEADING_NNW,
  HEADING_N3W,
} heading_t;
// TODO REMOVE
const char* heading_to_str(heading_t heading);

const ge_coord_t HEADING_MOVES[][3] = {
    // North quadrant
    [HEADING_N] = {{0, -1}, {0, -1}, {0, -1}},
    [HEADING_N3E] = {{0, -1}, {1, -1}, {0, -1}},
    [HEADING_NNE] = {{1, -1}, {0, -1}, {1, -1}},
    [HEADING_NE] = {{1, -1}, {1, -1}, {1, -1}},
    [HEADING_NEE] = {{1, -1}, {1, 0}, {1, -1}},
    [HEADING_NE3] = {{1, 0}, {1, -1}, {1, 0}},
    // East quadrant
    [HEADING_E] = {{1, 0}, {1, 0}, {1, 0}},
    [HEADING_SE3] = {{1, 0}, {1, 1}, {1, 0}},
    [HEADING_SEE] = {{1, 1}, {1, 0}, {1, 1}},
    [HEADING_SE] = {{1, 1}, {1, 1}, {1, 1}},
    [HEADING_SSE] = {{1, 1}, {0, 1}, {1, 1}},
    [HEADING_S3E] = {{0, 1}, {1, 1}, {0, 1}},
    // South quadrant
    [HEADING_S] = {{0, 1}, {0, 1}, {0, 1}},
    [HEADING_S3W] = {{0, 1}, {-1, 1}, {0, 1}},
    [HEADING_SSW] = {{-1, 1}, {0, 1}, {-1, 1}},
    [HEADING_SW] = {{-1, 1}, {-1, 1}, {-1, 1}},
    [HEADING_SWW] = {{-1, 1}, {-1, 0}, {-1, 1}},
    [HEADING_SW3] = {{-1, 0}, {-1, 1}, {-1, 0}},
    // West quadrant
    [HEADING_W] = {{-1, 0}, {-1, 0}, {-1, 0}},
    [HEADING_NW3] = {{-1, 0}, {-1, -1}, {-1, 0}},
    [HEADING_NWW] = {{-1, -1}, {-1, 0}, {-1, -1}},
    [HEADING_NW] = {{-1, -1}, {-1, -1}, {-1, -1}},
    [HEADING_NNW] = {{-1, -1}, {0, -1}, {-1, -1}},
    [HEADING_N3W] = {{0, -1}, {-1, -1}, {0, -1}},
};

const heading_t BOUNCE_NORTH[] = {
    // North quadrant
    [HEADING_N] = HEADING_N,
    [HEADING_N3E] = HEADING_N3E,
    [HEADING_NNE] = HEADING_NNE,
    [HEADING_NE] = HEADING_NE,
    [HEADING_NEE] = HEADING_NEE,
    [HEADING_NE3] = HEADING_NE3,
    // East quadrant
    [HEADING_E] = HEADING_E,
    [HEADING_SE3] = HEADING_NE3,
    [HEADING_SEE] = HEADING_NEE,
    [HEADING_SE] = HEADING_NE,
    [HEADING_SSE] = HEADING_NNE,
    [HEADING_S3E] = HEADING_N3E,
    // South quadrant
    [HEADING_S] = HEADING_N,
    [HEADING_S3W] = HEADING_N3W,
    [HEADING_SSW] = HEADING_NNW,
    [HEADING_SW] = HEADING_NW,
    [HEADING_SWW] = HEADING_NWW,
    [HEADING_SW3] = HEADING_NW3,
    // West quadrant
    [HEADING_W] = HEADING_W,
    [HEADING_NW3] = HEADING_NW3,
    [HEADING_NWW] = HEADING_NWW,
    [HEADING_NW] = HEADING_NW,
    [HEADING_NNW] = HEADING_NNW,
    [HEADING_N3W] = HEADING_N3W,
};

const heading_t BOUNCE_EAST[] = {
    // North quadrant
    [HEADING_N] = HEADING_N,
    [HEADING_N3E] = HEADING_N3E,
    [HEADING_NNE] = HEADING_NNE,
    [HEADING_NE] = HEADING_NE,
    [HEADING_NEE] = HEADING_NEE,
    [HEADING_NE3] = HEADING_NE3,
    // East quadrant
    [HEADING_E] = HEADING_E,
    [HEADING_SE3] = HEADING_SE3,
    [HEADING_SEE] = HEADING_SEE,
    [HEADING_SE] = HEADING_SE,
    [HEADING_SSE] = HEADING_SSE,
    [HEADING_S3E] = HEADING_S3E,
    // South quadrant
    [HEADING_S] = HEADING_S,
    [HEADING_S3W] = HEADING_S3E,
    [HEADING_SSW] = HEADING_SSE,
    [HEADING_SW] = HEADING_SE,
    [HEADING_SWW] = HEADING_SEE,
    [HEADING_SW3] = HEADING_SE3,
    // West quadrant
    [HEADING_W] = HEADING_E,
    [HEADING_NW3] = HEADING_NE3,
    [HEADING_NWW] = HEADING_NEE,
    [HEADING_NW] = HEADING_NE,
    [HEADING_NNW] = HEADING_NNE,
    [HEADING_N3W] = HEADING_N3E,
};

const heading_t BOUNCE_SOUTH[] = {
    // North quadrant
    [HEADING_N] = HEADING_S,
    [HEADING_N3E] = HEADING_S3E,
    [HEADING_NNE] = HEADING_SSE,
    [HEADING_NE] = HEADING_SE,
    [HEADING_NEE] = HEADING_SEE,
    [HEADING_NE3] = HEADING_SE3,
    // East quadrant
    [HEADING_E] = HEADING_E,
    [HEADING_SE3] = HEADING_SE3,
    [HEADING_SEE] = HEADING_SEE,
    [HEADING_SE] = HEADING_SE,
    [HEADING_SSE] = HEADING_SSE,
    [HEADING_S3E] = HEADING_S3E,
    // South quadrant
    [HEADING_S] = HEADING_S,
    [HEADING_S3W] = HEADING_S3W,
    [HEADING_SSW] = HEADING_SSW,
    [HEADING_SW] = HEADING_SW,
    [HEADING_SWW] = HEADING_SWW,
    [HEADING_SW3] = HEADING_SW3,
    // West quadrant
    [HEADING_W] = HEADING_W,
    [HEADING_NW3] = HEADING_SW3,
    [HEADING_NWW] = HEADING_SWW,
    [HEADING_NW] = HEADING_SW,
    [HEADING_NNW] = HEADING_SSW,
    [HEADING_N3W] = HEADING_S3W,
};

const heading_t BOUNCE_WEST[] = {
    // North quadrant
    [HEADING_N] = HEADING_N,
    [HEADING_N3E] = HEADING_N3W,
    [HEADING_NNE] = HEADING_NNW,
    [HEADING_NE] = HEADING_NW,
    [HEADING_NEE] = HEADING_NWW,
    [HEADING_NE3] = HEADING_NW3,
    // East quadrant
    [HEADING_E] = HEADING_W,
    [HEADING_SE3] = HEADING_SW3,
    [HEADING_SEE] = HEADING_SWW,
    [HEADING_SE] = HEADING_SW,
    [HEADING_SSE] = HEADING_SSW,
    [HEADING_S3E] = HEADING_S3W,
    // South quadrant
    [HEADING_S] = HEADING_S,
    [HEADING_S3W] = HEADING_S3W,
    [HEADING_SSW] = HEADING_SSW,
    [HEADING_SW] = HEADING_SW,
    [HEADING_SWW] = HEADING_SWW,
    [HEADING_SW3] = HEADING_SW3,
    // West quadrant
    [HEADING_W] = HEADING_W,
    [HEADING_NW3] = HEADING_NW3,
    [HEADING_NWW] = HEADING_NWW,
    [HEADING_NW] = HEADING_NW,
    [HEADING_NNW] = HEADING_NNW,
    [HEADING_N3W] = HEADING_N3W,
};

const heading_t HIT_UP[] = {
    // North quadrant
    [HEADING_N] = HEADING_N,
    [HEADING_N3E] = HEADING_N3E,  // Do not hit vertical!
    [HEADING_NNE] = HEADING_N3E,
    [HEADING_NE] = HEADING_NNE,
    [HEADING_NEE] = HEADING_NE,
    [HEADING_NE3] = HEADING_NEE,
    // East quadrant
    [HEADING_E] = HEADING_NE3,
    [HEADING_SE3] = HEADING_E,
    [HEADING_SEE] = HEADING_SE3,
    [HEADING_SE] = HEADING_SEE,
    [HEADING_SSE] = HEADING_SE,
    [HEADING_S3E] = HEADING_SSE,
    // South quadrant
    [HEADING_S] = HEADING_S,
    [HEADING_S3W] = HEADING_SSW,
    [HEADING_SSW] = HEADING_SW,
    [HEADING_SW] = HEADING_SWW,
    [HEADING_SWW] = HEADING_SW3,
    [HEADING_SW3] = HEADING_W,
    // West quadrant
    [HEADING_W] = HEADING_NW3,
    [HEADING_NW3] = HEADING_NWW,
    [HEADING_NWW] = HEADING_NW,
    [HEADING_NW] = HEADING_NNW,
    [HEADING_NNW] = HEADING_N3W,
    [HEADING_N3W] = HEADING_N3W,  // Do not hit vertical!
};

const heading_t HIT_DOWN[] = {
    // North quadrant
    [HEADING_N] = HEADING_N,
    [HEADING_N3E] = HEADING_NNE,
    [HEADING_NNE] = HEADING_NE,
    [HEADING_NE] = HEADING_NEE,
    [HEADING_NEE] = HEADING_NE3,
    [HEADING_NE3] = HEADING_E,
    // East quadrant
    [HEADING_E] = HEADING_SE3,
    [HEADING_SE3] = HEADING_SEE,
    [HEADING_SEE] = HEADING_SE,
    [HEADING_SE] = HEADING_SSE,
    [HEADING_SSE] = HEADING_S3E,
    [HEADING_S3E] = HEADING_S3E,  // Do not hit vertical!
    // South quadrant
    [HEADING_S] = HEADING_S,
    [HEADING_S3W] = HEADING_S3W,  // Do not hit vertical!
    [HEADING_SSW] = HEADING_S3W,
    [HEADING_SW] = HEADING_SSW,
    [HEADING_SWW] = HEADING_SW,
    [HEADING_SW3] = HEADING_SWW,
    // West quadrant
    [HEADING_W] = HEADING_SW3,
    [HEADING_NW3] = HEADING_W,
    [HEADING_NWW] = HEADING_NW3,
    [HEADING_NW] = HEADING_NWW,
    [HEADING_NNW] = HEADING_NW,
    [HEADING_N3W] = HEADING_NNW,
};

typedef struct user_data {
  // Paddle 1 data
  ge_coord_t paddle1_coord;
  size_t paddle1_size;
  int paddle1_vel;
  // Paddle 2 data
  ge_coord_t paddle2_coord;
  size_t paddle2_size;
  ge_coord_t ball_coord;
  int paddle2_vel;
  // Ball data, move index rangs 0 to 2
  heading_t ball_heading;
  size_t ball_move_index;
  // Scoring data
  size_t player1_score;
  size_t player2_score;
  bool reset_ball;
  uint32_t last_reset_time_ms;
} user_data_t;

void fade_grid(ge_grid_t* restrict grid)
{
  for (size_t jj = 0; jj < ge_grid_get_height(grid); ++jj) {
    for (size_t ii = 0; ii < ge_grid_get_width(grid); ++ii) {
      const ge_coord_t coord = {ii, jj};
      const uint8_t value = ge_grid_get_coord(grid, coord);
      if (value >= VALUE_ON) {
        ge_grid_set_coord(grid, coord, VALUE_FADE);
      }
      else {
        const uint8_t fade_value = (value > VALUE_FADE_DELTA ? value - VALUE_FADE_DELTA : 0);
        ge_grid_set_coord(grid, coord, fade_value);
      }
    }
  }
}

void draw_horizontal_bumper(ge_grid_t* restrict grid, size_t y, size_t width)
{
  for (size_t ii = 0; ii < width; ++ii) {
    ge_grid_set_coord(grid, (ge_coord_t){ii, y}, VALUE_ON);
  }
}

void draw_ball_bumper(ge_grid_t* restrict grid, ge_coord_t bumper_coord, size_t width)
{
  for (size_t jj = 0; jj < width; ++jj) {
    for (size_t ii = 0; ii < width; ++ii) {
      const ge_coord_t bumper_offset = {ii - width / 2, jj - width / 2};
      ge_grid_set_coord(grid, ge_coord_add(bumper_coord, bumper_offset), VALUE_ON);
    }
  }
}

void draw_paddle(ge_grid_t* restrict grid, ge_coord_t paddle_coord, size_t paddle_size)
{
  if (paddle_size == 0) return;
  const size_t odd_paddle_size = (paddle_size % 2 != 0 ? paddle_size : paddle_size - 1);
  const size_t half_paddle_size = (odd_paddle_size - 1) / 2;
  // Get the number of single and double hit factor pixels
  const size_t hit_x2_size = half_paddle_size / 3;
  const size_t hit_x1_size = hit_x2_size;
  const size_t hit_x0_size = half_paddle_size - hit_x2_size - hit_x1_size;
  // Neutral hit pixels (always includes the middle pixel)
  ge_grid_set_coord_wrapped(grid, paddle_coord, VALUE_PADDLE_HIT_N);
  for (size_t ii = 0; ii < hit_x0_size; ++ii) {
    const ge_coord_t paddle_offset = {0, ii};
    ge_grid_set_coord_wrapped(grid, ge_coord_sub(paddle_coord, paddle_offset), VALUE_PADDLE_HIT_N);
    ge_grid_set_coord_wrapped(grid, ge_coord_add(paddle_coord, paddle_offset), VALUE_PADDLE_HIT_N);
  }
  // Hit x1 pixels
  for (size_t ii = 0; ii < hit_x1_size; ++ii) {
    const ge_coord_t paddle_offset = {0, hit_x0_size + ii};
    ge_grid_set_coord_wrapped(grid, ge_coord_sub(paddle_coord, paddle_offset), VALUE_PADDLE_HIT_U);
    ge_grid_set_coord_wrapped(grid, ge_coord_add(paddle_coord, paddle_offset), VALUE_PADDLE_HIT_D);
  }
  // Hit x2 pixels
  for (size_t ii = 0; ii < hit_x2_size; ++ii) {
    const ge_coord_t paddle_offset = {0, hit_x0_size + hit_x1_size + ii};
    ge_grid_set_coord_wrapped(grid, ge_coord_sub(paddle_coord, paddle_offset), VALUE_PADDLE_HIT_UU);
    ge_grid_set_coord_wrapped(grid, ge_coord_add(paddle_coord, paddle_offset), VALUE_PADDLE_HIT_DD);
  }
}

void draw_ball(ge_grid_t* restrict grid, ge_coord_t ball_coord)
{
  ge_grid_set_coord_wrapped(grid, ball_coord, VALUE_ON);
}

void draw_score(ge_grid_t* restrict grid, ge_coord_t score_coord, size_t score)
{
  static ge_coord_t score_coord_buffer[500];
  static char score_str[10];
  snprintf(score_str, 10, "%lu", score);
  const size_t num_coords = ge_utils_str_coords(score_str, score_coord, score_coord_buffer, 500);
  for (size_t ii = 0; ii < num_coords; ++ii) {
    const ge_coord_t glyph_coord = score_coord_buffer[ii];
    ge_grid_set_coord_wrapped(grid, glyph_coord, VALUE_FADE);
  }
}

heading_t heading_apply_hit(heading_t heading, uint8_t value)
{
  switch (value) {
  case VALUE_PADDLE_HIT_UU:
    heading = HIT_UP[heading];
    heading = HIT_UP[heading];
    break;
  case VALUE_PADDLE_HIT_U:
    heading = HIT_UP[heading];
    break;
  case VALUE_PADDLE_HIT_D:
    heading = HIT_DOWN[heading];
    break;
  case VALUE_PADDLE_HIT_DD:
    heading = HIT_DOWN[heading];
    heading = HIT_DOWN[heading];
    break;
  default:
    // Do nothing
    break;
  }
  return heading;
}

void process_grid(ge_grid_t* restrict grid, user_data_t* restrict user_data)
{
  // Get width, height, etc
  const size_t width = ge_grid_get_width(grid);
  const size_t height = ge_grid_get_height(grid);
  // Fade the grid and draw the bumpers
  fade_grid(grid);
  draw_horizontal_bumper(grid, 0, width);
  draw_horizontal_bumper(grid, height - 1, width);
  draw_ball_bumper(grid, (ge_coord_t){width * 3 / 8, height * 3 / 8}, 3);
  draw_ball_bumper(grid, (ge_coord_t){width * 3 / 8, height * 5 / 8}, 3);
  draw_ball_bumper(grid, (ge_coord_t){width * 5 / 8, height * 3 / 8}, 3);
  draw_ball_bumper(grid, (ge_coord_t){width * 5 / 8, height * 5 / 8}, 3);
  // Draw the score board
  draw_score(grid, (ge_coord_t){width * 2 / 8 - 8, height / 2 - 4}, user_data->player1_score);
  draw_score(grid, (ge_coord_t){width * 6 / 8 - 8, height / 2 - 4}, user_data->player2_score);
}

void process_one_paddle(ge_grid_t* restrict grid, ge_coord_t* paddle_coord, size_t paddle_size,
                        int paddle_vel)
{
  // Get width of the grid
  const size_t height = ge_grid_get_height(grid);
  // Get the actual paddle size and the new coord
  const size_t odd_paddle_size = (paddle_size % 2 != 0 ? paddle_size : paddle_size - 1);
  const size_t half_paddle_size = (odd_paddle_size - 1) / 2;
  const ge_coord_t new_coord = ge_coord_add(*paddle_coord, (ge_coord_t){0, paddle_vel});
  // Check the boundries before applying the move
  if (new_coord.y - half_paddle_size + 1 > 0 && new_coord.y + half_paddle_size < height) {
    *paddle_coord = new_coord;
  }
  // Draw the paddle to the screen
  draw_paddle(grid, *paddle_coord, paddle_size);
}

void process_paddles(ge_grid_t* restrict grid, user_data_t* restrict user_data)
{
  process_one_paddle(grid, &user_data->paddle1_coord, user_data->paddle1_size,
                     user_data->paddle1_vel);
  process_one_paddle(grid, &user_data->paddle2_coord, user_data->paddle2_size,
                     user_data->paddle2_vel);
}

void process_ball(ge_grid_t* restrict grid, user_data_t* restrict user_data)
{
  // Update ball's coord from the current move, according to the heading
  if (user_data->ball_heading != HEADING_NONE) {
    const ge_coord_t* const ball_moves = HEADING_MOVES[user_data->ball_heading];
    const ge_coord_t ball_move = ball_moves[user_data->ball_move_index];
    user_data->ball_move_index = ++user_data->ball_move_index % 3;
    user_data->ball_coord = ge_coord_add(user_data->ball_coord, ball_move);
  }
  // Draw the ball
  draw_ball(grid, user_data->ball_coord);
}

void process_bounces(ge_grid_t* restrict grid, user_data_t* restrict user_data)
{
  // Check for bounces with the paddles, bumpers, etc
  const ge_coord_t neighbor_north = ge_coord_add(user_data->ball_coord, (ge_coord_t){0, -1});
  const ge_coord_t neighbor_east = ge_coord_add(user_data->ball_coord, (ge_coord_t){1, 0});
  const ge_coord_t neighbor_south = ge_coord_add(user_data->ball_coord, (ge_coord_t){0, 1});
  const ge_coord_t neighbor_west = ge_coord_add(user_data->ball_coord, (ge_coord_t){-1, 0});
  const uint8_t value_north = ge_grid_get_coord_wrapped(grid, neighbor_north);
  const uint8_t value_east = ge_grid_get_coord_wrapped(grid, neighbor_east);
  const uint8_t value_south = ge_grid_get_coord_wrapped(grid, neighbor_south);
  const uint8_t value_west = ge_grid_get_coord_wrapped(grid, neighbor_west);
  if (value_north >= VALUE_ON) {
    user_data->ball_heading = BOUNCE_SOUTH[user_data->ball_heading];
    user_data->ball_heading = heading_apply_hit(user_data->ball_heading, value_north);
  }
  else if (value_south >= VALUE_ON) {
    user_data->ball_heading = BOUNCE_NORTH[user_data->ball_heading];
    user_data->ball_heading = heading_apply_hit(user_data->ball_heading, value_south);
  }
  if (value_east >= VALUE_ON) {
    user_data->ball_heading = BOUNCE_WEST[user_data->ball_heading];
    user_data->ball_heading = heading_apply_hit(user_data->ball_heading, value_east);
  }
  else if (value_west >= VALUE_ON) {
    user_data->ball_heading = BOUNCE_EAST[user_data->ball_heading];
    user_data->ball_heading = heading_apply_hit(user_data->ball_heading, value_west);
  }
}

void process_score_and_reset(ge_grid_t* restrict grid, user_data_t* restrict user_data,
                             uint32_t time_ms)
{
  // Get width and height
  const size_t width = ge_grid_get_width(grid);
  const size_t height = ge_grid_get_height(grid);
  // Check for the ball getting a goal
  if (user_data->ball_coord.x == 0) {
    user_data->player2_score++;
    user_data->reset_ball = true;
  }
  else if (user_data->ball_coord.x == ((ptrdiff_t) width) - 1) {
    user_data->player1_score++;
    user_data->reset_ball = true;
  }
  // Handle the reset as neccessary
  if (user_data->reset_ball) {
    user_data->ball_coord = (ge_coord_t){width / 2, height / 2};
    user_data->ball_heading = HEADING_NONE;
    user_data->last_reset_time_ms = time_ms;
    user_data->reset_ball = false;
  }
  if (user_data->last_reset_time_ms != 0) {
    if (time_ms - user_data->last_reset_time_ms > RESET_TIME_MS) {
      user_data->ball_heading = (rand() % 2 == 0 ? HEADING_E : HEADING_W);
      user_data->last_reset_time_ms = 0;
    }
  }
}

void process_paddle_events(user_data_t* restrict user_data, const ge_event_t* restrict event)
{
  // Process the events to move the paddles
  if (event->type == GE_EVENT_KEYDOWN) {
    // GE_LOG_INFO("KEYDOWN = %s", ge_keycode_to_str(event->keydown_data.keycode));
    if (event->keydown_data.keycode == GE_KEYCODE_F
        || event->keydown_data.keycode == GE_KEYCODE_W) {
      user_data->paddle1_vel = -1;
    }
    else if (event->keydown_data.keycode == GE_KEYCODE_S) {
      user_data->paddle1_vel = 1;
    }
    if (event->keydown_data.keycode == GE_KEYCODE_U
        || event->keydown_data.keycode == GE_KEYCODE_I) {
      user_data->paddle2_vel = -1;
    }
    else if (event->keydown_data.keycode == GE_KEYCODE_E
             || event->keydown_data.keycode == GE_KEYCODE_K) {
      user_data->paddle2_vel = 1;
    }
  }
  else if (event->type == GE_EVENT_KEYUP) {
    // GE_LOG_INFO("KEYUP = %s", ge_keycode_to_str(event->keyup_data.keycode));
    if (event->keyup_data.keycode == GE_KEYCODE_F || event->keyup_data.keycode == GE_KEYCODE_W) {
      if (user_data->paddle1_vel == -1) user_data->paddle1_vel = 0;
    }
    else if (event->keyup_data.keycode == GE_KEYCODE_S) {
      if (user_data->paddle1_vel == 1) user_data->paddle1_vel = 0;
    }
    if (event->keyup_data.keycode == GE_KEYCODE_U || event->keyup_data.keycode == GE_KEYCODE_I) {
      if (user_data->paddle2_vel == -1) user_data->paddle2_vel = 0;
    }
    else if (event->keyup_data.keycode == GE_KEYCODE_E
             || event->keyup_data.keycode == GE_KEYCODE_K) {
      if (user_data->paddle2_vel == 1) user_data->paddle2_vel = 0;
    }
  }
}

void pong_loop_func(ge_grid_t* restrict grid, void* restrict user_data_, uint32_t time_ms)
{
  // Cast the user data back to the right type
  user_data_t* user_data = (user_data_t*) user_data_;
  // Do all the processing for the game loop
  process_grid(grid, user_data);
  process_paddles(grid, user_data);
  process_ball(grid, user_data);
  process_bounces(grid, user_data);
  process_score_and_reset(grid, user_data, time_ms);
}

void pong_event_func(ge_grid_t* restrict grid, void* restrict user_data_, uint32_t time_ms,
                     const ge_event_t* restrict event)
{
  (void) grid;
  (void) time_ms;
  // Cast the user data back to the right type
  user_data_t* user_data = (user_data_t*) user_data_;
  // Process paddle events
  process_paddle_events(user_data, event);
}

int main(void)
{
  // Initialize random number generator
  srand(time(NULL));
  // Define grid width and height
  const size_t width = 150;
  const size_t height = 70;
  ge_grid_t* grid = ge_grid_create(width, height);
  // User data to track state, etc
  user_data_t user_data = {
      .paddle1_coord = (ge_coord_t){10, height / 2},
      .paddle1_size = 15,
      .paddle1_vel = 0,
      .paddle2_coord = (ge_coord_t){width - 11, height / 2},
      .paddle2_size = 15,
      .paddle2_vel = 0,
      .ball_coord = (ge_coord_t){-1, -1},
      .ball_heading = HEADING_NONE,
      .ball_move_index = 0,
      .player1_score = 0,
      .player2_score = 0,
      .reset_ball = true,
      .last_reset_time_ms = 0,
  };
  // The EZ loop data
  ez_loop_data_t ez_loop_data = {
      .grid = grid,
      .user_data = &user_data,
      .loop_func = pong_loop_func,
      .event_func = pong_event_func,
  };
  // RUN THE LOOP!
  return ge_ez_loop(&ez_loop_data);
}
