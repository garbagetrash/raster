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
