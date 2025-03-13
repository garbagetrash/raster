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


const char* channel = "ipc:///dev/shm/ipc";
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
    sprintf(pathname, "%s/%s", dir, filename);
    printf("path: %s\n", pathname);
}

typedef struct Raster {
    int width;
    int height;
    float min_value;
    float max_value;
    int yidx;
    Color* pixels;
} Raster;

// Allocates Color*, up to user to free
Raster new_raster(int width, int height)
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

    Raster r = {
        .width = width,   
        .height = height,   
        .min_value = FLT_MAX,
        .max_value = FLT_MIN,
        .yidx = 0,   
        .pixels = pixels
    };
    return r;
}

void free_raster(Raster* r)
{
    free(r->pixels);
}

// Updates `pixels` by pushing a horizontal line of width pixels
void push_line(const float* line_of_pixels, Raster* raster, float* colormap)
{
    int idx;
    int buffer_size = raster->width * raster->height;
    for (int x = 0; x < raster->width; x++)
    {
        float value = line_of_pixels[x];
        if (value > raster->max_value)
        {
            raster->max_value = value;
        }
        if (value < raster->min_value)
        {
            raster->min_value = value;
        }
    }

    float range = raster->max_value - raster->min_value;
    for (int x = 0; x < raster->width; x++)
    {
        float value = line_of_pixels[x];
        // Apply colormap here
        int idx = (int)(230.0 * ((value - raster->min_value) / (range + 1e-6)));
        Color c = {
            255 * colormap[3 * idx],
            255 * colormap[3 * idx + 1],
            255 * colormap[3 * idx + 2],
            255
        };
        idx = (raster->yidx * raster->width + x);
        raster->pixels[idx] = c;
    }
    (raster->yidx)++;
    raster->yidx %= raster->height;
}

// Converges to N(0, 1)
float randn(int n)
{
    float output = 0.0f;
    for (size_t i = 0; i < n; i++)
    {
        output += ((float)rand() / RAND_MAX);
    }
    return sqrtf(12.0f * n) * (output - 0.5f);
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
            buffer[i] = -abs((width / 2.0f) - i) + randn(20);
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
}

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    bool simulated_input = false;
    int c;
    float* colormap = (float*)inferno_srgb_floats;

    while ((c = getopt(argc, argv, "sh:w:c:")) != -1)
    {
        switch (c)
        {
            case 's':
                simulated_input = true;
                break;
            case 'c':
                char* color_choice = optarg;
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
    InitWindow(width, height, "Raster");
    width = GetScreenWidth();
    height = GetScreenHeight();
    Raster raster = new_raster(width, height);
    SetTargetFPS(60);

    float* buffer = (float*)calloc(sizeof(float), width);

    RenderTexture2D rtex = LoadRenderTexture(width, height);
    Color* line_of_pixels = (Color*)calloc(sizeof(Color), raster.width);
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

            free_raster(&raster);
            raster = new_raster(width, height);

            rtex = LoadRenderTexture(width, height);

            free(line_of_pixels);
            line_of_pixels = (Color*)calloc(sizeof(Color), raster.width);

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
            push_line(buffer, &raster, colormap);
        }

        // Render all the data we have
        Color* pixels = raster.pixels;
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
        }

        // Draw
        BeginDrawing();

        ClearBackground(BLACK);

        // Actual raster
        DrawTexture(rtex.texture, 0, 0, WHITE);
        DrawLine(0, raster.yidx, width, raster.yidx, YELLOW);

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Draw rectangle for select box
            Vector2 start = {
                min(click_start.x, mouse_pos.x),
                min(click_start.y, mouse_pos.y),
            };
            Vector2 size = {
                abs(click_start.x - mouse_pos.x),
                abs(click_start.y - mouse_pos.y),
            };
            Rectangle select = { start.x, start.y, size.x, size.y };
            DrawRectangleRec(select, Fade(WHITE, 0.35f));
            DrawRectangleLinesEx(select, 1.0, YELLOW);
            char start_text[20];
            sprintf(start_text, "(%d, %d)", (int)click_start.x, (int)click_start.y);
            DrawText(start_text, (int)click_start.x + 5, (int)click_start.y - 15, 10, YELLOW);
            char mouse_text[20];
            sprintf(mouse_text, "(%d, %d)", (int)mouse_pos.x, (int)mouse_pos.y);
            DrawText(mouse_text, (int)mouse_pos.x + 5, (int)mouse_pos.y - 15, 10, YELLOW);
            char width_text[6];
            sprintf(width_text, "%d", (int)size.x);
            DrawText(width_text, (int)(start.x + 0.5 * size.x - 5), (int)start.y - 15, 10, YELLOW);
            char height_text[6];
            sprintf(height_text, "%d", (int)size.y);
            DrawText(height_text, (int)start.x - 30, (int)(start.y + 0.5 * size.y - 5), 10, YELLOW);
        } else {
            // Mouse crosshair
            DrawLine(0, mouse_pos.y, width, mouse_pos.y, YELLOW);
            DrawLine(mouse_pos.x, 0, mouse_pos.x, height, YELLOW);
            if ((width - mouse_pos.x) < 40) {
                // Close to right edge
                char xpos[10];
                sprintf(xpos, "X: %i", (int)(mouse_pos.x));
                DrawText(xpos, (int)mouse_pos.x - 40, height - 10, 10, YELLOW);
            } else {
                char xpos[10];
                sprintf(xpos, "X: %i", (int)(mouse_pos.x));
                DrawText(xpos, (int)mouse_pos.x + 5, height - 10, 10, YELLOW);
            }
            if ((height - mouse_pos.y) < 20) {
                // Close to bottom edge
                char ypos[10];
                sprintf(ypos, "Y: %i", (int)(mouse_pos.y));
                DrawText(ypos, 5, (int)mouse_pos.y - 15, 10, YELLOW);
            } else {
                char ypos[10];
                sprintf(ypos, "Y: %i", (int)(mouse_pos.y));
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
    free(line_of_pixels);
    free_raster(&raster);
    UnloadRenderTexture(rtex);
    CloseWindow();
    free(buffer);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    unlink("/dev/shm/ipc");

    return 0;
}
