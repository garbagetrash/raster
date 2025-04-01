#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raylib.h"
#include "common.h"
#include "grayscale_colormap.h"
#include "inferno_colormap.h"
#include "viridis_colormap.h"
#include "turbo_colormap.h"


// TODO: Need to think more about this, ideally zoom operates like a stack.
// Draw rectangle to push a new zoom, right click to pop a zoom.
Screen screen = {
    .width = 480,
    .height = 640,
    .zoom_stack = {
        (Zoom) {
            .logical_width = 1.0f,
            .logical_height = 1.0f,
            .logical_minx = -0.5f,
            .logical_miny = 0.0f,
        },
    },
    .zlevel = 0,
};

typedef enum {
    MAIN,
    HELP,
} ActiveScreen;


Tag global_tags[16] = { 0 };
size_t ntags = 0;
ActiveScreen active_screen = MAIN;


// TODO: Implement zoom on actual waterfall/texture
typedef struct Waterfall {
    int width;
    int height;
    float min_value;
    float max_value;
    int yidx;
    Color* pixels;
} Waterfall;

// Allocates Color*, up to user to free
Waterfall new_waterfall(int width, int height)
{
    int buffer_size = width * height;
    Color* pixels = (Color*)calloc(sizeof(Color), buffer_size);
    int idx;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            idx = (y + width + x) % buffer_size;
            pixels[idx] = BLACK;
        }
    }

    Waterfall r = {
        .width = width,   
        .height = height,   
        .min_value = FLT_MAX,
        .max_value = FLT_MIN,
        .yidx = 0,   
        .pixels = pixels
    };
    return r;
}

void free_waterfall(Waterfall* r)
{
    free(r->pixels);
}

// Updates `pixels` by pushing a horizontal line of width pixels
void push_line(const float* line_of_pixels, Waterfall* waterfall, float* colormap)
{
    int idx;
    int buffer_size = waterfall->width * waterfall->height;
    for (int x = 0; x < waterfall->width; x++)
    {
        float value = line_of_pixels[x];
        if (value > waterfall->max_value)
        {
            waterfall->max_value = value;
        }
        if (value < waterfall->min_value)
        {
            waterfall->min_value = value;
        }
    }

    float range = waterfall->max_value - waterfall->min_value;
    for (int x = 0; x < waterfall->width; x++)
    {
        float value = line_of_pixels[x];
        // Apply colormap here
        int idx = (int)(230.0 * ((value - waterfall->min_value) / (range + 1e-6)));
        Color c = {
            255 * colormap[3 * idx],
            255 * colormap[3 * idx + 1],
            255 * colormap[3 * idx + 2],
            255
        };
        idx = (waterfall->yidx * waterfall->width + x);
        waterfall->pixels[idx] = c;
    }
    (waterfall->yidx)++;
    waterfall->yidx %= waterfall->height;
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    int c;
    float* colormap = (float*)inferno_srgb_floats;
    char* color_choice = NULL;

    while ((c = getopt(argc, argv, "sf:h:c:")) != -1)
    {
        switch (c)
        {
            case 'c':
                color_choice = optarg;
                if (strncmp(color_choice, "inferno", 7) == 0) {
                    colormap = (float*)inferno_srgb_floats;
                } else if (strncmp(color_choice, "viridis", 7) == 0) {
                    colormap = (float*)viridis_srgb_floats;
                } else if (strncmp(color_choice, "turbo", 5) == 0) {
                    colormap = (float*)turbo_srgb_floats;
                } else if (strncmp(color_choice, "gray", 4) == 0) {
                    colormap = (float*)grayscale_srgb_floats;
                } else if (strncmp(color_choice, "grey", 4) == 0) {
                    colormap = (float*)grayscale_srgb_floats;
                }
                break;
            default:
                abort();
        }
    }

    printf("colormap choice: %s\n", color_choice);

    // Now set up our GUI
    InitWindow(screen.width, screen.height, "Waterfall");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Waterfall waterfall = new_waterfall(screen.width, screen.height);
    SetTargetFPS(60);
    Font font = LoadFont("resources/fonts/pixelplay.png");

    float* buffer = (float*)calloc(sizeof(float), screen.width);

    RenderTexture2D rtex = LoadRenderTexture(screen.width, screen.height);
    Color* line_of_pixels = (Color*)calloc(sizeof(Color), waterfall.width);
    Vector2 click_start = { 0, 0 };
    Vector2 click_end = { 0, 0 };
    int last_mouse = 0; // 0 - not pressed, 1 - pressed

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    while (!WindowShouldClose())
    {
        // Update
        if (IsWindowResized())
        {
            // Window resized, so reset width/height
            screen.width = GetScreenWidth();
            screen.height = GetScreenHeight();

            free_waterfall(&waterfall);
            waterfall = new_waterfall(screen.width, screen.height);

            rtex = LoadRenderTexture(screen.width, screen.height);

            free(line_of_pixels);
            line_of_pixels = (Color*)calloc(sizeof(Color), waterfall.width);

            free(buffer);
            buffer = (float*)calloc(sizeof(float), screen.width);
        }

        while (1)
        {
            // Receive all queued up data and push to Waterfall before rendering frame
            ssize_t nbytes = read(0, buffer, sizeof(float)*screen.width);
            if (nbytes == 0)
            {
                // EOF
                break;
            } else if (nbytes == -1) {
                // If we don't have data don't block or bail, just allow loop to go
                // along so UI remains responsive.
                if (errno != EAGAIN)
                {
                    perror("read");
                }
                break;
            }

            // This also applies colormap
            push_line(buffer, &waterfall, colormap);
        }

        // Render all the data we have
        Color* pixels = waterfall.pixels;
        UpdateTexture(rtex.texture, pixels);

        Vector2 mouse_pos = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            last_mouse = 1;
            click_start = mouse_pos;
            printf("click start at: (%f, %f)\n", click_start.x, click_start.y);
        } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            last_mouse = 0;
            click_end = mouse_pos;
            printf("click end at: (%f, %f)\n", click_end.x, click_end.y);

            // Zoom to rectangle (click_start, click_end)
            push_zoom_stack(&screen, click_start, click_end);
        }

        // Right click to back out the zoom stack 1 level
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            if (screen.zlevel > 0) {
                screen.zlevel--;
            }
        }

        // Tags
        if (IsKeyPressed(KEY_T) && active_screen == MAIN && ntags < 16)
        {
            Vector2 tagpos = to_logical(mouse_pos, &screen);
            printf("New Tag: (%f, %f)\n", tagpos.x, tagpos.y);

            Tag t = {
                .logical_position = tagpos,
                .label = "",
            };
            snprintf(t.label, 22, "(%f, %f)", tagpos.x, tagpos.y);
            global_tags[ntags] = t;
            ntags++;
        } else if (IsKeyPressed(KEY_Y)) {
            ntags = 0;
        } else if (IsKeyPressed(KEY_SPACE)) {
            if (active_screen == MAIN) {
                active_screen = HELP;
            } else if (active_screen == HELP) {
                active_screen = MAIN;
            }
        }

        // Draw
        BeginDrawing();

        ClearBackground(BLACK);

        if (active_screen == HELP) {
            DrawText("Controls", 20, 10, 20, WHITE);
            DrawText("t   - Draw Tag", 20, 40, 14, WHITE);
            DrawText("y   - Clear Tags", 20, 60, 14, WHITE);
            DrawText("Click and Drag to zoom", 20, 80, 14, WHITE);
            DrawText("Esc - Quit", 20, 100, 14, WHITE);

            DrawText("Tags", screen.width / 2, 10, 20, WHITE);
            for (size_t i = 0; i < ntags; i++)
            {
                Vector2 tpos = { .x = screen.width / 2, .y = 40 + 20 * i };
                DrawTextEx(font, global_tags[i].label, tpos, 14, 4.0f, WHITE);
            }
        } else {
            // Actual waterfall
            DrawTexture(rtex.texture, 0, 0, WHITE);
            DrawLine(0, waterfall.yidx, screen.width, waterfall.yidx, YELLOW);

            // Draw tagged positions
            draw_tags(global_tags, ntags, &screen);

            // Draw crosshairs and select box
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                // Draw rectangle for select box
                draw_mouse_drag_rectangle(click_start, mouse_pos, &screen);
            } else {
                // Mouse crosshair
                draw_mouse_crosshair(mouse_pos, &screen);
            }

            // Info panel
            draw_info_panel(&screen);
        }
        EndDrawing();
    }

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | ~O_NONBLOCK);

    // Clean up
    free(line_of_pixels);
    free_waterfall(&waterfall);
    UnloadRenderTexture(rtex);
    CloseWindow();
    free(buffer);
    UnloadFont(font);

    return 0;
}
