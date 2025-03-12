#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"


void get_path(const char* filename, char* pathname)
{
    char *dir = RESOURCES_DIR;
    printf("dir: %s\n", dir);
    strcpy(pathname, dir);
    strcat(pathname, filename);
    printf("path: %s\n", pathname);
}

typedef struct Raster {
    int width;
    int height;
    int yidx;
    Color* pixels;
} Raster;

// Allocates Color*, up to user to free
Raster new_raster(int width, int height)
{
    int buffer_size = 2 * width * height;
    Color* pixels = (Color*)calloc(sizeof(Color), buffer_size);
    int idx;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            idx = (y + width + x) % buffer_size;
            pixels[idx] = BLACK;
            idx = ((y + height) * width + x) % buffer_size;
            pixels[idx] = BLACK;
        }
    }

    Raster r = {
        .width = width,   
        .height = height,   
        .yidx = 0,   
        .pixels = pixels
    };
    return r;
}

// Updates `pixels` by pushing a horizontal line of width pixels
void push_line(const Color* line_of_pixels, Raster* raster)
{
    int idx;
    int buffer_size = 2 * raster->width * raster->height;
    for (int x = 0; x < raster->width; x++)
    {
        idx = (raster->yidx * raster->width + x) % buffer_size;
        raster->pixels[idx] = line_of_pixels[x];
        idx = ((raster->yidx + raster->height) * raster->width + x) % buffer_size;
        raster->pixels[idx] = line_of_pixels[x];
    }
    (raster->yidx)++;
    raster->yidx %= raster->height;
}

int main()
{
    const int width = 1024;
    const int height = 800;

    InitWindow(width, height, "Raster");

    Raster raster = new_raster(width, height);

    SetTargetFPS(60);

    long long cntr = 0;
    RenderTexture2D rtex = LoadRenderTexture(width, height);
    while (!WindowShouldClose())
    {
        // Update
        Color* line_of_pixels = (Color*)calloc(sizeof(Color), raster.width);
        for (int i = 0; i < raster.width; i++)
        {
            line_of_pixels[i] = (Color) { cntr%256, cntr%256, cntr%256, 255 };
        }
        cntr++;
        push_line(line_of_pixels, &raster);
        free(line_of_pixels);
        Color* pixels = &(raster.pixels[raster.yidx * raster.width]);
        UpdateTexture(rtex.texture, pixels);

        // Draw
        BeginDrawing();

        ClearBackground(RAYWHITE);

        DrawTexture(rtex.texture, 0, 0, WHITE);

        DrawRectangle(width - 210, 0, 210, 40, Fade(WHITE, 0.7f));
        DrawText("RASTER", width - 200, 10, 20, BLACK);
        DrawFPS(width - 90, 10);

        EndDrawing();
    }

    UnloadRenderTexture(rtex);

    CloseWindow();

    return 0;
}
