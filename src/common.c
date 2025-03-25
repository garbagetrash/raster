#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "raylib.h"
#include "common.h"


float min(float x, float y)
{
    if (x < y) return x;
    else return y;
}

float max(float x, float y)
{
    if (x > y) return x;
    else return y;
}

void get_path(const char* filename, char* pathname)
{
    char *dir = RESOURCES_DIR;
    printf("dir: %s\n", dir);
    snprintf(pathname, 512, "%s/%s", dir, filename);
    printf("path: %s\n", pathname);
}

// Box-Muller
float randn()
{
    float u1 = ((float)rand() / RAND_MAX);
    float u2 = ((float)rand() / RAND_MAX);
    return sqrtf(-2 * log(u1)) * cos(2 * M_PI * u2);
}

// From logical space x, y: [0.0, 1.0) to screen space [0, # pixels).
Vector2 to_pixels(Vector2 logical, Screen* screen)
{
    float normx = (logical.x - screen->logical_minx) / screen->logical_width;
    float normy = (logical.y - screen->logical_miny) / screen->logical_height;
    Vector2 point = { normx * screen->width, (1.0f - normy) * screen->height };
    return point;
}

// From screen space x, y: [0, # pixels) to logical space [0.0, 1.0).
Vector2 to_logical(Vector2 pixels, Screen* screen)
{
    float normx = pixels.x / screen->width;
    float normy = pixels.y / screen->height;
    Vector2 point = {
        normx * screen->logical_width + screen->logical_minx,
        (1.0f - normy) * screen->logical_height + screen->logical_miny
    };
    return point;
}

void draw_mouse_crosshair(Vector2 mouse_pos, Screen* screen)
{
    DrawLine(0, mouse_pos.y, screen->width, mouse_pos.y, YELLOW);
    DrawLine(mouse_pos.x, 0, mouse_pos.x, screen->height, YELLOW);

    Vector2 _mouse_pos = to_logical(mouse_pos, screen);

    char xpos[10];
    snprintf(xpos, 10, "X: %f", _mouse_pos.x);
    if ((screen->width - mouse_pos.x) < 50) {
        // Close to right edge
        DrawText(xpos, (int)mouse_pos.x - 50, screen->height - 10, 10, YELLOW);
    } else {
        DrawText(xpos, (int)mouse_pos.x + 5, screen->height - 10, 10, YELLOW);
    }

    char ypos[10];
    snprintf(ypos, 10, "Y: %f", _mouse_pos.y);
    if ((screen->height - mouse_pos.y) < 20) {
        // Close to bottom edge
        DrawText(ypos, 5, (int)mouse_pos.y - 15, 10, YELLOW);
    } else {
        DrawText(ypos, 5, (int)mouse_pos.y + 5, 10, YELLOW);
    }
}

void draw_mouse_drag_rectangle(Vector2 click_start, Vector2 mouse_pos, Screen* screen)
{
    Vector2 start = {
        min(click_start.x, mouse_pos.x),
        min(click_start.y, mouse_pos.y),
    };
    Vector2 size = {
        fabsf(click_start.x - mouse_pos.x),
        fabsf(click_start.y - mouse_pos.y),
    };
    Rectangle select = { start.x, start.y, size.x, size.y };
    DrawRectangleRec(select, Fade(WHITE, 0.35f));
    DrawRectangleLinesEx(select, 1.0, YELLOW);

    Vector2 _click_start = to_logical(click_start, screen);
    Vector2 _mouse_pos = to_logical(mouse_pos, screen);
    Vector2 _size = {
        fabsf(_click_start.x - _mouse_pos.x),
        fabsf(_click_start.y - _mouse_pos.y),
    };

    char start_text[22];
    snprintf(start_text, 22, "(%f, %f)", _click_start.x, _click_start.y);
    DrawText(start_text, (int)click_start.x + 5, (int)click_start.y - 15, 10, YELLOW);
    char mouse_text[22];
    snprintf(mouse_text, 22, "(%f, %f)", _mouse_pos.x, _mouse_pos.y);
    DrawText(mouse_text, (int)mouse_pos.x + 5, (int)mouse_pos.y - 15, 10, YELLOW);
    char width_text[9];
    snprintf(width_text, 9, "%f", _size.x);
    DrawText(width_text, (int)(start.x + 0.5 * size.x - 20), (int)start.y + 5, 10, YELLOW);
    char height_text[9];
    snprintf(height_text, 9, "%f", _size.y);
    DrawText(height_text, (int)start.x - 45, (int)(start.y + 0.5 * size.y - 5), 10, YELLOW);
}

void free_byte_vec(ByteVec* bvec)
{
    if (bvec->bytes)
    {
        free(bvec->bytes);
    }
}

ByteVec load_file(const char* filename)
{
    FILE* fid = fopen(filename, "rb");
    if (!fid)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if (fseek(fid, 0, SEEK_END) == -1)
    {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    size_t nbytes = ftell(fid);
    if (fseek(fid, 0, SEEK_SET) == -1)
    {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    char* buffer = (char*)malloc(nbytes);
    size_t nread = fread(buffer, sizeof(char), nbytes, fid);
    if (nread != nbytes)
    {
        fprintf(stderr, "fread() failed: %zu\n", nread);
        exit(EXIT_FAILURE);
    }
    fclose(fid);

    ByteVec bvec = {
        .nbytes = nbytes,
        .bytes = buffer,
    };

    return bvec;
}

void free_vec_f32(VecF32* v)
{
    if (v->points)
    {
        free(v->points);
    }
}

void free_vec_cf32(VecCf32* v)
{
    if (v->points)
    {
        free(v->points);
    }
}

void convert_i8_f32(const int8_t* in, float* out, size_t nelements)
{
    for (size_t i = 0; i < nelements; i++)
    {
        out[i] = (float)in[i];
    }
}

void convert_i16_f32(const int16_t* in, float* out, size_t nelements)
{
    for (size_t i = 0; i < nelements; i++)
    {
        out[i] = (float)in[i];
    }
}

void convert_i32_f32(const int32_t* in, float* out, size_t nelements)
{
    for (size_t i = 0; i < nelements; i++)
    {
        out[i] = (float)in[i];
    }
}

void convert_i64_f32(const int64_t* in, float* out, size_t nelements)
{
    for (size_t i = 0; i < nelements; i++)
    {
        out[i] = (float)in[i];
    }
}

void convert_f64_f32(const double* in, float* out, size_t nelements)
{
    for (size_t i = 0; i < nelements; i++)
    {
        out[i] = (float)in[i];
    }
}

VecF32 load_file_real(const char* filename, DataType type)
{
    size_t element_size = 4;
    switch (type)
    {
        case I8: element_size = 1; break;
        case I16: element_size = 2; break;
        case I32: element_size = 4; break;
        case I64: element_size = 8; break;
        case F32: element_size = 4; break;
        case F64: element_size = 8; break;
        default:
            fprintf(stderr, "DataType not supported");
            exit(EXIT_FAILURE);
    }

    FILE* fid = fopen(filename, "rb");
    if (!fid)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if (fseek(fid, 0, SEEK_END) == -1)
    {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    size_t nbytes = ftell(fid);
    size_t nelements = nbytes / element_size;
    if (fseek(fid, 0, SEEK_SET) == -1)
    {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    void* _buffer = malloc(nelements * element_size);
    size_t nread = fread(_buffer, element_size, nelements, fid);
    if (nread != nelements)
    {
        fprintf(stderr, "fread() failed: %zu\n", nread);
        exit(EXIT_FAILURE);
    }
    fclose(fid);

    float* buffer = (float*)malloc(nelements * sizeof(float));
    switch (type)
    {
        case I8: convert_i8_f32((int8_t*)_buffer, buffer, nelements); break;
        case I16: convert_i16_f32((int16_t*)_buffer, buffer, nelements); break;
        case I32: convert_i32_f32((int32_t*)_buffer, buffer, nelements); break;
        case I64: convert_i64_f32((int64_t*)_buffer, buffer, nelements); break;
        case F32: buffer = (float*)buffer; break;
        case F64: convert_f64_f32((double*)_buffer, buffer, nelements); break;
        default:
            fprintf(stderr, "DataType not supported");
            exit(EXIT_FAILURE);
    }

    if (type != F32)
    {
        free(_buffer);
    }

    VecF32 v = {
        .npoints = nelements,
        .points = buffer,
    };

    return v;
}

VecCf32 load_file_complex(const char* filename, DataType type)
{
    size_t element_size = 4;
    switch (type)
    {
        case Ci8: element_size = 1; break;
        case Ci16: element_size = 2; break;
        case Ci32: element_size = 4; break;
        case Ci64: element_size = 8; break;
        case Cf32: element_size = 4; break;
        case Cf64: element_size = 8; break;
        default:
            fprintf(stderr, "DataType not supported");
            exit(EXIT_FAILURE);
    }

    FILE* fid = fopen(filename, "rb");
    if (!fid)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if (fseek(fid, 0, SEEK_END) == -1)
    {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    size_t nbytes = ftell(fid);
    size_t nelements = nbytes / (2 * element_size);
    if (fseek(fid, 0, SEEK_SET) == -1)
    {
        perror("fseek");
        exit(EXIT_FAILURE);
    }
    void* _buffer = malloc(2 * nelements * element_size);
    size_t nread = fread(_buffer, element_size, 2 * nelements, fid);
    if (nread != 2 * nelements)
    {
        fprintf(stderr, "fread() failed: %zu\n", nread);
        exit(EXIT_FAILURE);
    }
    fclose(fid);

    float complex* buffer = (float complex*)malloc(nelements * sizeof(float complex));
    switch (type)
    {
        case Ci8:  convert_i8_f32((int8_t*)_buffer, (float*)buffer, 2 * nelements); break;
        case Ci16: convert_i16_f32((int16_t*)_buffer, (float*)buffer, 2 * nelements); break;
        case Ci32: convert_i32_f32((int32_t*)_buffer, (float*)buffer, 2 * nelements); break;
        case Ci64: convert_i64_f32((int64_t*)_buffer, (float*)buffer, 2 * nelements); break;
        case Cf32: buffer = (float complex*)buffer; break;
        case Cf64: convert_f64_f32((double*)_buffer, (float*)buffer, 2 * nelements); break;
        default:
            fprintf(stderr, "DataType not supported");
            exit(EXIT_FAILURE);
    }

    if (type != Cf32)
    {
        free(_buffer);
    }

    VecCf32 v = {
        .npoints = nelements,
        .points = buffer,
    };

    return v;
}

void draw_info_panel(Screen *screen)
{
    DrawRectangle(screen->width - 170, 0, 170, 56, Fade(WHITE, 0.7f));
    DrawText("Space for controls", screen->width - 160, 8, 16, BLACK);
}

void draw_tags(Tag* tags, size_t ntags, Screen* screen)
{
    for (size_t i = 0; i < ntags; i++)
    {
        Vector2 pixels = to_pixels(tags[i].logical_position, screen);
        DrawCircleLinesV(pixels, 2, YELLOW);
        DrawText(tags[i].label, pixels.x - 52, pixels.y - 15, 10, YELLOW);
    }
}
