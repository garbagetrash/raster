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


Screen screen = {
    .width = 640,
    .height = 480,
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


typedef struct Plot {
    float min_value;
    float max_value;
    int32_t npoints;
    Vector2* points;
} Plot;

// Allocates Color*, up to user to free
Plot new_plot(uint32_t npoints)
{
    Vector2* buffer = (Vector2*)calloc(sizeof(Vector2), npoints);
    Plot p = {
        .min_value = FLT_MAX,
        .max_value = FLT_MIN,
        .npoints = npoints,
        .points = buffer
    };
    return p;
}

void free_plot(Plot* p)
{
    if (p->points != NULL)
    {
        free(p->points);
    }
}

// Shallow copy, just passing the pointer along.
void update_plot(float* points, uint64_t npoints, Plot* plot)
{
    plot->npoints = npoints;
    float dx = (float)screen.width / (plot->npoints - 1);

    // Update plot range
    for (int i = 0; i < plot->npoints; i++)
    {
        float y = points[i];
        if (y > plot->max_value)
        {
            plot->max_value = y;
        }
        if (y < plot->min_value)
        {
            plot->min_value = y;
        }
    }

    float x = 0.0f;
    float range = plot->max_value - plot->min_value + 1e-6;
    for (int i = 0; i < plot->npoints; i++)
    {
        // Map each point onto logical space [0, 1.0)
        float y = (points[i] - plot->min_value) / range;

        Vector2 xy = { x, (1.0f - y) * screen.height };
        plot->points[i] = xy;
        x += dx;
    }
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    // Now set up our GUI
    InitWindow(screen.width, screen.height, "Plot");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Plot plot = new_plot(screen.width);
    SetTargetFPS(60);
    Font font = LoadFont("resources/fonts/pixelplay.png");

    float* points_buffer = (float*)calloc(sizeof(float), screen.width);

    RenderTexture2D rtex = LoadRenderTexture(screen.width, screen.height);
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

            free_plot(&plot);
            plot = new_plot(screen.width);

            free(points_buffer);
            points_buffer = (float*)calloc(sizeof(float), screen.width);

            rtex = LoadRenderTexture(screen.width, screen.height);
        }

        int nelements = 0;
        while (1)
        {
            // Receive all queued up data and push to Raster before rendering frame
            ssize_t nbytes = read(0, points_buffer, sizeof(float)*screen.width);
            if (nbytes == 0)
            {
                // EOF
                break;
            } else if (nbytes == -1) {
                if (errno == EAGAIN)
                {
                    break;
                } else {
                    // Error
                    perror("read");
                    break;
                }
            }
            nelements = nbytes / sizeof(float);
            if (nelements > screen.width) {
                // Received data got truncated down to buffer size
                nelements = screen.width;
            }

            // Only draw the most recent line
            update_plot(points_buffer, nelements, &plot);
        }

        // TODO: Render all the data we have to a texture?
        //Color* pixels = plot.pixels;
        //UpdateTexture(rtex.texture, pixels);

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
            // Actual plot
            DrawLineStrip(plot.points, plot.npoints, WHITE);

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
    free(points_buffer);
    free_plot(&plot);
    UnloadRenderTexture(rtex);
    CloseWindow();
    UnloadFont(font);

    return 0;
}
