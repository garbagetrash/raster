#pragma once

#include <complex.h>
#include <stdint.h>

#include "raylib.h"

typedef struct VecF32 {
    uint64_t npoints;
    float* points;
} VecF32;

void free_vec_f32(VecF32* v);

typedef struct VecCf32 {
    uint64_t npoints;
    float complex* points;
} VecCf32;

void free_vec_cf32(VecCf32* v);

typedef struct ByteVec {
    uint64_t nbytes;
    char* bytes;
} ByteVec;

void free_byte_vec(ByteVec* bvec);

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


typedef struct Tag {
    Vector2 logical_position;
    char label[22];
} Tag;


typedef enum {
    U8,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
    Ci8,
    Ci16,
    Ci32,
    Ci64,
    Cf32,
    Cf64,
} DataType;


float min(float x, float y);
float max(float x, float y);
float randn();
void get_path(const char* filename, char* pathname);
Vector2 to_pixels(Vector2 logical, Screen* screen);
Vector2 to_logical(Vector2 pixels, Screen* screen);
ByteVec load_file_bytes(const char* filename);
VecF32 load_file_real(const char* filename, DataType type);
VecCf32 load_file_complex(const char* filename, DataType type);

void draw_mouse_crosshair(Vector2 mouse_pos, Screen* screen);
void draw_mouse_drag_rectangle(Vector2 click_start, Vector2 mouse_pos, Screen* screen);
void draw_info_panel(Screen* screen);
void draw_tags(Tag* tags, size_t ntags, Screen* screen);
