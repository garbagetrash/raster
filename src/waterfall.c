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


Screen screen = {
    .width = 480,
    .height = 640,
    .logical_width = 1.0f,
    .logical_height = 1.0f,
    .logical_minx = -0.5f,
    .logical_miny = 0.0f,
};


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

void* simulated_input_thread()
{
    // Set up ZMQ publisher
    void* context = zmq_ctx_new();
    void* publisher = zmq_socket(context, ZMQ_PUB);
    if (zmq_bind(publisher, "ipc://ipc") == -1)
    {
        perror("zmq_bind");
        abort();
    }

    float* buffer = (float*)calloc(sizeof(float), 4 * 1920);

    // Loop forever on sending some data
    while (1)
    {
        for (int i = 0; i < screen.width; i++)
        {
            buffer[i] = -fabsf((screen.width/ 2.0f) - i) + randn();
        }

        if (zmq_send(publisher, buffer, sizeof(float) * screen.width, 0) == -1)
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

typedef enum InputType {
    File,
    Zmq,
} InputType;

int main(int argc, char *argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    bool simulated_input = false;
    int c;
    float* colormap = (float*)inferno_srgb_floats;
    char* color_choice = NULL;
    char* filename = NULL;
    char* hostname = "ipc://ipc";
    InputType type = Zmq;

    while ((c = getopt(argc, argv, "sf:h:c:")) != -1)
    {
        switch (c)
        {
            case 's':
                simulated_input = true;
                break;
            case 'f':
                filename = optarg;
                type = File;
                break;
            case 'h':
                hostname = optarg;
                type = Zmq;
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

    printf("simulated input: %i\n", simulated_input);
    if (type == File)
    {
        printf("filename: %s\n", filename);
    } else if (type == Zmq) {
        printf("hostname: %s\n", hostname);
    }
    printf("colormap choice: %s\n", color_choice);

    // If simulated input, spin off the simulator thread
    pthread_t tid;
    if (simulated_input)
    {
        if (type == Zmq)
        {
            if (pthread_create(&tid, NULL, &simulated_input_thread, NULL) != 0)
            {
                perror("pthread_create");
                return -1;
            }
        } else if (type == File) {
            // Create a `test.dat` to load up
            FILE* fid = fopen("test.dat", "wb");
            size_t sz = 16 * 1024 *  1024;
            float* temp = (float*)malloc(sizeof(float) * sz);
            for (size_t i = 0; i < sz; i++)
            {
                temp[i] = randn();
            }
            fwrite(temp, sizeof(float), sz, fid);
            free(temp);
            fclose(fid);
            filename = "test.dat";
        }
    }

    // Now set up our GUI
    InitWindow(screen.width, screen.height, "Waterfall");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Waterfall waterfall = new_waterfall(screen.width, screen.height);
    SetTargetFPS(60);

    void* context;
    void* subscriber;
    if (type == Zmq)
    {
        // First set up the ZMQ subscriber socket
        context = zmq_ctx_new();
        subscriber = zmq_socket(context, ZMQ_SUB);
        if (zmq_connect(subscriber, hostname) == -1)
        {
            perror("zmq_connect");
            return -1;
        }

        if (zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0) == -1)
        {
            perror("zmq_setsockopt");
            return -1;
        }
    } else if (type == File) {
        // Load up the file and push it into the waterfall plot
        VecF32 fvec = load_file_f32(filename);
        for (size_t i = 0; i < fvec.npoints / waterfall.width; i++)
        {
            push_line(&(fvec.points[waterfall.width * i]), &waterfall, colormap);
        }
        free_vec_f32(&fvec);
    }

    float* buffer = (float*)calloc(sizeof(float), screen.width);

    RenderTexture2D rtex = LoadRenderTexture(screen.width, screen.height);
    Color* line_of_pixels = (Color*)calloc(sizeof(Color), waterfall.width);
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

            free_waterfall(&waterfall);
            waterfall = new_waterfall(screen.width, screen.height);

            rtex = LoadRenderTexture(screen.width, screen.height);

            free(line_of_pixels);
            line_of_pixels = (Color*)calloc(sizeof(Color), waterfall.width);

            free(buffer);
            buffer = (float*)calloc(sizeof(float), screen.width);
        }

        if (type == Zmq)
        {
            while (1)
            {
                // Receive all queued up data and push to Waterfall before rendering frame
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
                push_line(buffer, &waterfall, colormap);
            }
        } else if (type == File && IsWindowResized()) {
            // Load up the file and push it into the waterfall plot
            VecF32 fvec = load_file_f32(filename);
            for (size_t i = 0; i < fvec.npoints / waterfall.width; i++)
            {
                push_line(&(fvec.points[waterfall.width * i]), &waterfall, colormap);
            }
            free_vec_f32(&fvec);
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
            // TODO: Zoom to rectangle (click_start, click_end)
        }

        // Draw
        BeginDrawing();

        ClearBackground(BLACK);

        // Actual waterfall
        DrawTexture(rtex.texture, 0, 0, WHITE);
        if (type == Zmq) {
            DrawLine(0, waterfall.yidx, screen.width, waterfall.yidx, YELLOW);
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            // Draw rectangle for select box
            draw_mouse_drag_rectangle(click_start, mouse_pos, &screen);
        } else {
            // Mouse crosshair
            draw_mouse_crosshair(mouse_pos, &screen);
        }

        // Info panel
        DrawRectangle(screen.width - 250, 0, 250, 40, Fade(WHITE, 0.7f));
        DrawText("WATERFALL", screen.width - 240, 10, 20, BLACK);
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
    free(line_of_pixels);
    free_waterfall(&waterfall);
    UnloadRenderTexture(rtex);
    CloseWindow();
    free(buffer);
    if (type == Zmq)
    {
        zmq_close(subscriber);
        zmq_ctx_destroy(context);
        unlink("/dev/shm/ipc");
    }

    return 0;
}
