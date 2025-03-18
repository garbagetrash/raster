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


const int NTRACES = 64;
const int TRACE_WIDTH = 256;
const char* channel = "ipc://ipc";
Screen screen = {
    .width = 480,
    .height = 640,
    .logical_width = 1.0f,
    .logical_height = 1.0f,
    .logical_minx = -0.5f,
    .logical_miny = 0.0f,
};


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
    for (int x = 0; x < raster1d->trace_width; x++)
    {
        float value = trace[x];
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

    int idx;
    for (int x = 0; x < raster1d->trace_width; x++)
    {
        float value = trace[x];
        // Apply colormap here
        idx = (raster1d->tidx * raster1d->trace_width + x);
        Vector2 pt = { x, value };
        raster1d->traces[idx] = to_pixels(pt, screen);
    }
    (raster1d->tidx)++;
    raster1d->tidx %= raster1d->ntraces;
}

void draw_raster1d(Raster1d* raster1d)
{
    float dvalue = 255.0 / raster1d->ntraces;
    int idx = raster1d->tidx;
    for (int i = 0; i < raster1d->ntraces; i++)
    {
        int value = (int)((i + 1) * dvalue);
        //Color c = { value, value, value, 255 };
        Color c = { 255, 255, 255, value };
        DrawLineStrip(&(raster1d->traces[raster1d->trace_width * idx]), raster1d->trace_width, c);
        idx++;
        idx %= raster1d->ntraces;
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
        if (randn() > 1.0f)
        {
            for (int i = 0; i < TRACE_WIDTH; i++)
            {
                buffer[i] = TRACE_WIDTH / 2.0f - fabsf((TRACE_WIDTH / 2.0f) - i) + 3.0f * randn();
            }
        } else {
            for (int i = 0; i < TRACE_WIDTH; i++)
            {
                buffer[i] = 3.0f * randn();
            }
        }

        if (zmq_send(publisher, buffer, sizeof(float) * TRACE_WIDTH, 0) == -1)
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

    while ((c = getopt(argc, argv, "sh:w:c:")) != -1)
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
    InitWindow(screen.width, screen.height, "Raster1d");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Raster1d raster1d = new_raster1d(NTRACES, TRACE_WIDTH, &screen);
    screen.logical_width = raster1d.trace_width;
    screen.logical_minx = 0.0f;
    SetTargetFPS(60);

    float* buffer = (float*)calloc(sizeof(float), screen.width);

    Vector2 click_start = { 0, 0 };
    Vector2 click_end = { 0, 0 };
    int last_mouse = 0; // 0 - not pressed, 1 - pressed

    while (!WindowShouldClose())
    {
        // Update
        if (IsWindowResized())
        {
            // Window resized, so reset width/height
            screen.width = GetScreenWidth();
            screen.height = GetScreenHeight();

            free_raster1d(&raster1d);
            raster1d = new_raster1d(NTRACES, TRACE_WIDTH, &screen);
            screen.logical_width = raster1d.trace_width;

            free(buffer);
            buffer = (float*)calloc(sizeof(float), screen.width);
        }

        while (1)
        {
            // Receive all queued up data and push to Raster1d before rendering frame
            if (zmq_recv(subscriber, buffer, sizeof(float) * screen.width, ZMQ_DONTWAIT) == -1)
            {
                // If we don't have data don't block or bail, just allow loop to go
                // along so UI remains responsive.
                if (errno != EAGAIN)
                {
                    perror("zmq_recv");
                    return -1;
                }
                break;
            }

            // This also applies colormap
            push_trace(buffer, &raster1d, &screen);
            screen.logical_miny = raster1d.min_value;
            screen.logical_height = raster1d.max_value - raster1d.min_value;
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
        }

        // Draw
        BeginDrawing();

        ClearBackground(BLACK);

        // Actual raster1d
        draw_raster1d(&raster1d);

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Draw rectangle for select box
            draw_mouse_drag_rectangle(click_start, mouse_pos, &screen);
        } else {
            // Mouse crosshair
            draw_mouse_crosshair(mouse_pos, &screen);
        }

        // Info panel
        DrawRectangle(screen.width - 250, 0, 250, 40, Fade(WHITE, 0.7f));
        DrawText("RASTER 1D", screen.width - 240, 10, 20, BLACK);
        DrawFPS(screen.width - 90, 10);

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
    free_raster1d(&raster1d);
    CloseWindow();
    free(buffer);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    unlink("/dev/shm/ipc");

    return 0;
}
