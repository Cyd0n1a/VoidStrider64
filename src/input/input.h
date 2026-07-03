#pragma once
#include <stdbool.h>

typedef struct {
    float move_x, move_y;   /* analog stick, normalized to roughly [-1,1] */
    bool  btn_a;            /* held */
    bool  btn_z;            /* held */
    bool  btn_start;        /* pressed this poll */
    bool  a_press, b_press; /* pressed this poll (menus) */
    bool  c_up, c_down, c_left, c_right;          /* held (fire) */
    bool  d_up, d_down, d_left, d_right;          /* pressed (menus) */
} input_state_t;

void input_init(void);
void input_poll(void);
const input_state_t *input_get(void);
