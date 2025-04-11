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
const size_t N_FRAMES = 64;
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
void update_plot(const float* points, Screen* screen, Plot* plot)
{
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

    screen->zoom_stack[0].logical_miny = plot->min_value;
    float range = plot->max_value - plot->min_value;
    screen->zoom_stack[0].logical_height = range;
    screen->zoom_stack[0].logical_minx = 0.0;
    screen->zoom_stack[0].logical_width = (float)plot->npoints;
    if (screen->zlevel > 0)
    {
        range = screen->zoom_stack[screen->zlevel].logical_height;
    }

    // Little fudge just to ensure range stays > 0.0
    range += 1e-6;

    for (int i = 0; i < plot->npoints; i++)
    {
        Vector2 pt = { (float)i, points[i] };
        // Map each point onto logical space [0, 1.0)
        Vector2 xy = to_pixels(pt, screen);
        plot->points[i] = xy;
    }
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    int frame_size = 1024;
    int c;

    while ((c = getopt(argc, argv, "f:")) != -1)
    {
        switch (c)
        {
            case 'f':
                frame_size = atoi(optarg);
                break;
            default:
                abort();
        }
    }

    printf("frame size     : %d\n", frame_size);

    read_buffer = new_read_buffer(frame_size, N_FRAMES);

    // Now set up our GUI
    InitWindow(screen.width, screen.height, "Plot");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Plot plot = new_plot(frame_size);
    SetTargetFPS(60);
    Font font = LoadFont("resources/fonts/pixelplay.png");

    float* points_buffer = (float*)calloc(sizeof(float), frame_size);

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
        }

        size_t nbytes = 0;
        pthread_mutex_lock(&read_buffer_mutex);
        read_all_frames_from_buffer(read_buffer, (char*)points_buffer, frame_size * sizeof(float), &nbytes);
        pthread_mutex_unlock(&read_buffer_mutex);
        size_t nframes = nbytes / (read_buffer->frame_size * read_buffer->bytes_per_element);

        // Only draw the most recent line
        if (nframes > 0) {
            update_plot(points_buffer, &screen, &plot);
        }

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
    free(points_buffer);
    free_plot(&plot);
    CloseWindow();
    UnloadFont(font);
    free_read_buffer(read_buffer);

    return 0;
}
