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
Vector2 to_screenspace(Vector2 logical, int width, int height)
{
    Vector2 point = { (int)(logical.x * width), (int)(logical.y * height) };
    return point;
}

// From screen space x, y: [0, # pixels) to logical space [0.0, 1.0).
Vector2 to_logical(Vector2 screen, int width, int height)
{
    Vector2 point = { screen.x / width, screen.y / height };
    return point;
}

void draw_mouse_crosshair(Vector2 mouse_pos, int width, int height)
{
    DrawLine(0, mouse_pos.y, width, mouse_pos.y, YELLOW);
    DrawLine(mouse_pos.x, 0, mouse_pos.x, height, YELLOW);
    if ((width - mouse_pos.x) < 40) {
        // Close to right edge
        char xpos[10];
        snprintf(xpos, 10, "X: %i", (int)(mouse_pos.x));
        DrawText(xpos, (int)mouse_pos.x - 40, height - 10, 10, YELLOW);
    } else {
        char xpos[10];
        snprintf(xpos, 10, "X: %i", (int)(mouse_pos.x));
        DrawText(xpos, (int)mouse_pos.x + 5, height - 10, 10, YELLOW);
    }
    if ((height - mouse_pos.y) < 20) {
        // Close to bottom edge
        char ypos[10];
        snprintf(ypos, 10, "Y: %i", (int)(mouse_pos.y));
        DrawText(ypos, 5, (int)mouse_pos.y - 15, 10, YELLOW);
    } else {
        char ypos[10];
        snprintf(ypos, 10, "Y: %i", (int)(mouse_pos.y));
        DrawText(ypos, 5, (int)mouse_pos.y + 5, 10, YELLOW);
    }
}
