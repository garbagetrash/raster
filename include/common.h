#pragma once

#include <stdint.h>

#include "raylib.h"

typedef struct VecF32 {
    uint64_t npoints;
    float* points;
} VecF32;

// Logical is [0, 1) in x and y, first quadrant of a plot so top is y = 1
// bottom is y = 0.
typedef struct Screen {
    int width;  // pixels
    int height; // pixels
    float logical_width;
    float logical_height;
    float logical_minx;
    float logical_miny;
} Screen;

float min(float x, float y);
float max(float x, float y);
float randn();
void get_path(const char* filename, char* pathname);
Vector2 to_pixels(Vector2 logical, Screen* screen);
Vector2 to_logical(Vector2 pixels, Screen* screen);
void draw_mouse_crosshair(Vector2 mouse_pos, Screen* screen);
void draw_mouse_drag_rectangle(Vector2 click_start, Vector2 mouse_pos, Screen* screen);
