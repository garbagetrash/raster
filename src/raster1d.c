#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
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


// Global ReadBuffer and mutex
ReadBuffer* read_buffer = NULL;
pthread_mutex_t read_buffer_mutex;

void* spin_read()
{
    // Pipes are 65536 bytes max by default.
    ssize_t nbytes = 0;
    char* tmp = (char*)calloc(1, 65536);
    while (1)
    {
        nbytes = read(0, tmp, 65536);
        if (nbytes == 0)
        {
            // EOF
            read_buffer->hit_eof = true;
            printf("hit EOF\n");
            break;
        } else if (nbytes == -1) {
            perror("read");
            break;
        }
        pthread_mutex_lock(&read_buffer_mutex);
        write_to_buffer(tmp, nbytes, read_buffer);
        pthread_mutex_unlock(&read_buffer_mutex);
    }
    free(tmp);
    return NULL;
}


typedef struct Raster1d {
    int ntraces;
    int trace_width;
    float min_value;
    float max_value;
    int tidx;
    Vector2* traces;
} Raster1d;

// Allocates Color*, up to user to free
Raster1d new_raster1d(int ntraces, int trace_width, Screen* screen)
{
    Vector2* traces = (Vector2*)calloc(sizeof(Vector2), trace_width * ntraces);
    int idx;
    for (int i = 0; i < ntraces; i++)
    {
        for (int j = 0; j < trace_width; j++)
        {
            idx = trace_width * i + j;
            Vector2 pt = { j, 0.0f };
            traces[idx] = pt;
        }
    }

    Raster1d r = {
        .ntraces = ntraces,
        .trace_width = trace_width,
        .min_value = FLT_MAX,
        .max_value = FLT_MIN,
        .tidx = 0,   
        .traces = traces
    };
    return r;
}

void free_raster1d(Raster1d* r)
{
    free(r->traces);
}

// Updates `traces` by pushing a new line
void push_trace(const float* trace, Raster1d* raster1d, Screen* screen)
{
    // Update range
    for (int i = 0; i < raster1d->trace_width; i++)
    {
        float value = trace[i];
        if (value > raster1d->max_value)
        {
            raster1d->max_value = value;
        }
        if (value < raster1d->min_value)
        {
            raster1d->min_value = value;
        }
    }
    float range = raster1d->max_value - raster1d->min_value;
    screen->zoom_stack[0].logical_miny = raster1d->min_value;
    screen->zoom_stack[0].logical_height = range;
    screen->zoom_stack[0].logical_minx = 0.0;
    screen->zoom_stack[0].logical_width = (float)raster1d->trace_width;

    if (screen->zlevel > 0)
    {
        range = screen->zoom_stack[screen->zlevel].logical_height;
    }

    // Little fudge just to ensure range stays > 0.0
    range += 1e-6;

    int idx;
    for (int x = 0; x < raster1d->trace_width; x++)
    {
        float value = trace[x];
        idx = (raster1d->tidx * raster1d->trace_width + x);
        Vector2 pt = { x, value };
        raster1d->traces[idx] = to_pixels(pt, screen);
    }
    (raster1d->tidx)++;
    raster1d->tidx %= raster1d->ntraces;
}

void draw_raster1d(Raster1d* raster1d, float* colormap)
{
    float dvalue = 255.0 / raster1d->ntraces;
    int idx = raster1d->tidx;
    for (int i = 0; i < raster1d->ntraces; i++)
    {
        int value = (int)((i + 1) * dvalue);
        //Color c = { value, value, value, 255 };
        //Color c = { 255, 255, 255, value };
        Color c = {
            255 * colormap[3 * value],
            255 * colormap[3 * value + 1],
            255 * colormap[3 * value + 2],
            255
        };
        DrawLineStrip(&(raster1d->traces[raster1d->trace_width * idx]), raster1d->trace_width, c);
        idx++;
        idx %= raster1d->ntraces;
    }
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    int frame_size = 1024;
    int n_traces = 64;
    float* colormap = (float*)viridis_srgb_floats;
    char* color_choice = NULL;
    int c;

    while ((c = getopt(argc, argv, "f:n:c:")) != -1)
    {
        switch (c)
        {
            case 'f':
                frame_size = atoi(optarg);
                break;
            case 'n':
                n_traces = atoi(optarg);
                break;
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

    printf("frame size     : %d\n", frame_size);
    printf("# traces       : %d\n", n_traces);
    printf("colormap choice: %s\n", color_choice);

    read_buffer = new_read_buffer(frame_size, n_traces);

    // Now set up our GUI
    InitWindow(screen.width, screen.height, "Raster1d");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Raster1d raster1d = new_raster1d(n_traces, frame_size, &screen);
    screen.zoom_stack[0].logical_width = raster1d.trace_width;
    screen.zoom_stack[0].logical_minx = 0.0f;
    SetTargetFPS(60);
    Font font = LoadFont("resources/fonts/pixelplay.png");

    //float* buffer = (float*)calloc(sizeof(float), n_traces * frame_size);
    float* buffer = (float*)calloc(sizeof(float), frame_size);

    Vector2 click_start = { 0, 0 };
    Vector2 click_end = { 0, 0 };
    int last_mouse = 0; // 0 - not pressed, 1 - pressed

    pthread_t tid;
    if (pthread_create(&tid, NULL, &spin_read, NULL) != 0)
    {
        perror("pthread_create");
        return -1;
    }

    while (!WindowShouldClose())
    {
        // Update
        if (IsWindowResized())
        {
            // Window resized, so reset width/height
            screen.width = GetScreenWidth();
            screen.height = GetScreenHeight();

            free_raster1d(&raster1d);
            raster1d = new_raster1d(n_traces, frame_size, &screen);
            screen.zoom_stack[0].logical_width = raster1d.trace_width;
        }

        size_t nbytes = 0;
        pthread_mutex_lock(&read_buffer_mutex);
        //read_all_frames_from_buffer(read_buffer, (char*)buffer, n_traces * frame_size * sizeof(float), &nbytes);
        read_all_frames_from_buffer(read_buffer, (char*)buffer, frame_size * sizeof(float), &nbytes);
        pthread_mutex_unlock(&read_buffer_mutex);
        size_t nframes = nbytes / (read_buffer->frame_size * read_buffer->bytes_per_element);

        // This also applies colormap
        if (nframes > 0) {
            /* TODO: This is too slow...
            for (size_t i = 0; i < nframes; i++) {
                push_trace(&(buffer[i*frame_size]), &raster1d, &screen);
            }
            */
            push_trace(buffer, &raster1d, &screen); // NOTE: Just pushing _1_ of our frames received per 16.666 ms for now...
            screen.zoom_stack[0].logical_miny = raster1d.min_value;
            screen.zoom_stack[0].logical_height = raster1d.max_value - raster1d.min_value;
        }

        // Render all the data we have
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
            // Actual raster1d
            draw_raster1d(&raster1d, colormap);

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

    // Clean up
    if (pthread_cancel(tid) != 0)
    {
        perror("pthread_cancel");
        return -1;
    }
    if (pthread_join(tid, NULL) != 0)
    {
        perror("pthread_join");
        return -1;
    }
    free_raster1d(&raster1d);
    CloseWindow();
    free(buffer);
    UnloadFont(font);
    free_read_buffer(read_buffer);

    return 0;
}
