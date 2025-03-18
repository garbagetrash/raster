#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raylib.h"
#include "zmq.h"
#include "common.h"
#include "grayscale_colormap.h"
#include "inferno_colormap.h"
#include "viridis_colormap.h"
#include "turbo_colormap.h"


const char* channel = "ipc://ipc";
int width = 480;
int height = 640;


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
    float dx = (float)width / (plot->npoints - 1);

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

        Vector2 xy = { x, (1.0f - y) * height };
        plot->points[i] = xy;
        x += dx;
    }
}

void* simulated_input_thread()
{
    // Set up ZMQ publisher
    void* context = zmq_ctx_new();
    void* publisher = zmq_socket(context, ZMQ_PUB);
    if (zmq_bind(publisher, channel) == -1)
    {
        perror("zmq_bind");
        abort();
    }

    float* buffer = (float*)calloc(sizeof(float), 4 * 1920);

    // Loop forever on sending some data
    while (1)
    {
        for (int i = 0; i < width; i++)
        {
            buffer[i] = -fabsf((width / 2.0f) - i) + randn();
        }

        if (zmq_send(publisher, buffer, sizeof(float) * width, 0) == -1)
        {
            perror("zmq_send");
            break;
        }

        if (usleep(16000) == -1)
        {
            perror("usleep");
            break;
        }
    }

    // Cleanup
    free(buffer);
    zmq_close(publisher);
    zmq_ctx_destroy(context);

    return NULL;
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    bool simulated_input = false;
    int c;

    while ((c = getopt(argc, argv, "s")) != -1)
    {
        switch (c)
        {
            case 's':
                simulated_input = true;
                break;
            default:
                abort();
        }
    }

    printf("simulated input: %i\n", simulated_input);

    // If simulated input, spin off the simulator thread
    pthread_t tid;
    if (simulated_input)
    {
        if (pthread_create(&tid, NULL, &simulated_input_thread, NULL) != 0)
        {
            perror("pthread_create");
            return -1;
        }
    }

    // First set up the ZMQ subscriber socket
    void* context = zmq_ctx_new();
    void* subscriber = zmq_socket(context, ZMQ_SUB);
    if (zmq_connect(subscriber, channel) == -1)
    {
        perror("zmq_connect");
        return -1;
    }

    if (zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0) == -1)
    {
        perror("zmq_setsockopt");
        return -1;
    }

    // Now set up our GUI
    InitWindow(width, height, "Plot");
    width = GetScreenWidth();
    height = GetScreenHeight();
    Plot plot = new_plot(width);
    SetTargetFPS(60);

    float* points_buffer = (float*)calloc(sizeof(float), width);

    RenderTexture2D rtex = LoadRenderTexture(width, height);
    Vector2 click_start = { 0, 0 };
    Vector2 click_end = { 0, 0 };
    int last_mouse = 0; // 0 - not pressed, 1 - pressed

    while (!WindowShouldClose())
    {
        // Update
        if (IsWindowResized())
        {
            // Window resized, so reset width/height
            width = GetScreenWidth();
            height = GetScreenHeight();

            free_plot(&plot);
            plot = new_plot(width);

            free(points_buffer);
            points_buffer = (float*)calloc(sizeof(float), width);

            rtex = LoadRenderTexture(width, height);
        }

        int nbytes = 0;
        while (1)
        {
            // Receive all queued up data and push to Raster before rendering frame
            nbytes = zmq_recv(subscriber, points_buffer, sizeof(float) * width, ZMQ_DONTWAIT);
            if (nbytes == -1)
            {
                // If we don't have data don't block or bail, just allow loop to go
                // along so UI remains responsive.
                if (errno != EAGAIN)
                {
                    perror("zmq_recv");
                    return -1;
                }
                break;
            } else if (nbytes > sizeof(float) * width) {
                // Received data got truncated down to buffer size
                nbytes = sizeof(float) * width;
            }

            // Only draw the most recent line
            uint64_t npoints = nbytes / sizeof(float);
            update_plot(points_buffer, npoints, &plot);
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
        }

        // Draw
        BeginDrawing();

        ClearBackground(BLACK);

        // Actual plot
        DrawLineStrip(plot.points, plot.npoints, WHITE);

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Draw rectangle for select box
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
            char start_text[20];
            snprintf(start_text, 20, "(%d, %d)", (int)click_start.x, (int)click_start.y);
            DrawText(start_text, (int)click_start.x + 5, (int)click_start.y - 15, 10, YELLOW);
            char mouse_text[20];
            snprintf(mouse_text, 20, "(%d, %d)", (int)mouse_pos.x, (int)mouse_pos.y);
            DrawText(mouse_text, (int)mouse_pos.x + 5, (int)mouse_pos.y - 15, 10, YELLOW);
            char width_text[6];
            snprintf(width_text, 6, "%d", (int)size.x);
            DrawText(width_text, (int)(start.x + 0.5 * size.x - 5), (int)start.y - 15, 10, YELLOW);
            char height_text[6];
            snprintf(height_text, 6, "%d", (int)size.y);
            DrawText(height_text, (int)start.x - 30, (int)(start.y + 0.5 * size.y - 5), 10, YELLOW);
        } else {
            // Mouse crosshair
            draw_mouse_crosshair(mouse_pos, width, height);
        }

        // Info panel
        DrawRectangle(width - 210, 0, 210, 40, Fade(WHITE, 0.7f));
        DrawText("RASTER", width - 200, 10, 20, BLACK);
        DrawFPS(width - 90, 10);

        EndDrawing();
    }

    // Clean up
    if (simulated_input)
    {
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
    }
    free(points_buffer);
    free_plot(&plot);
    UnloadRenderTexture(rtex);
    CloseWindow();
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    unlink("/dev/shm/ipc");

    return 0;
}
