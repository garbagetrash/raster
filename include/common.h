#pragma once

#include <stdint.h>

#include "raylib.h"

typedef struct VecF32 {
    uint64_t npoints;
    float* points;
} VecF32;

float min(float x, float y);
float max(float x, float y);
float randn();
void get_path(const char* filename, char* pathname);
Vector2 to_screenspace(Vector2 logical, int width, int height);
Vector2 to_logical(Vector2 screen, int width, int height);
void draw_mouse_crosshair(Vector2 mouse_pos, int width, int height);
