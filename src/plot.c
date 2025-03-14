#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raylib.h"
#include "zmq.h"
#include "grayscale_colormap.h"
#include "inferno_colormap.h"
#include "viridis_colormap.h"
#include "turbo_colormap.h"


const char* channel = "ipc://ipc";
int width = 480;
int height = 640;


float min(float x, float y)
{
    if (x < y) return x;
    else return y;
}

void get_path(const char* filename, char* pathname)
{
    char *dir = RESOURCES_DIR;
    printf("dir: %s\n", dir);
    snprintf(pathname, 512, "%s/%s", dir, filename);
    printf("path: %s\n", pathname);
}

typedef struct Plot {
    uint32_t npoints;
    float min_value;
    float max_value;
    float* points;
} Plot;

// Allocates Color*, up to user to free
Plot new_plot(uint32_t npoints)
{
    float* points = (float*)calloc(sizeof(float), npoints);
    for (int i = 0; i < npoints; i++)
    {
        points[i] = 0.0;
    }

    Plot p = {
        .npoints = npoints,
        .min_value = FLT_MAX,
        .max_value = FLT_MIN,
        .points = points
    };
    return p;
}

void free_plot(Plot* p)
{
    free(p->points);
}

void push_line(const float* new_points, Plot* plot)
{
    for (int i = 0; i < plot->npoints; i++)
    {
        float value = new_points[i];
        if (value > plot->max_value)
        {
            plot->max_value = value;
        }
        if (value < plot->min_value)
        {
            plot->min_value = value;
        }
    }

    float range = plot->max_value - plot->min_value;
    memcpy(plot->points, new_points, plot->npoints);
}

// Box-Muller
float randn()
{
    float u1 = ((float)rand() / RAND_MAX);
    float u2 = ((float)rand() / RAND_MAX);
    return sqrtf(-2 * log(u1)) * cos(2 * M_PI * u2);
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

    float* buffer = (float*)calloc(sizeof(float), width);

    RenderTexture2D rtex = LoadRenderTexture(width, height);
    float* new_points = (float*)calloc(sizeof(float), plot.npoints);
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

            rtex = LoadRenderTexture(width, height);

            free(new_points);
            new_points = (float*)calloc(sizeof(float), plot.npoints);

            free(buffer);
            buffer = (float*)calloc(sizeof(float), width);
        }

        while (1)
        {
            // Receive all queued up data and push to Raster before rendering frame
            if (zmq_recv(subscriber, buffer, sizeof(float) * width, ZMQ_DONTWAIT) == -1)
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
            push_line(buffer, &plot);
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

        // TODO: Actual plot
        //DrawTexture(rtex.texture, 0, 0, WHITE);

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
    free(new_points);
    free_plot(&plot);
    UnloadRenderTexture(rtex);
    CloseWindow();
    free(buffer);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    unlink("/dev/shm/ipc");

    return 0;
}
