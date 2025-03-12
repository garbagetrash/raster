#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raylib.h"
#include "zmq.h"


const char* channel = "ipc:///dev/shm/ipc";


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

void* simulated_input_thread(void* arg)
{
    int buffer_size = *((int*)arg);

    // Set up ZMQ publisher
    void* context = zmq_ctx_new();
    void* publisher = zmq_socket(context, ZMQ_PUB);
    if (zmq_bind(publisher, channel) == -1)
    {
        perror("zmq_bind");
        abort();
    }

    float* buffer = (float*)calloc(sizeof(float), buffer_size);

    // Loop forever on sending some data
    int cntr = 0;
    while (1)
    {
        for (int i = 0; i < buffer_size; i++)
        {
            buffer[i] = (float)cntr;
        }
        cntr++;
        cntr %= 256;

        if (zmq_send(publisher, buffer, sizeof(float) * buffer_size, 0) == -1)
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
    int width = 1024;
    int height = 800;
    bool simulated_input = false;
    int c;

    while ((c = getopt(argc, argv, "sh:w:")) != -1)
    {
        switch (c)
        {
            case 's':
                simulated_input = true;
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 'w':
                width = atoi(optarg);
                break;
            default:
                abort();
        }
    }

    printf("simulated input: %i\n", simulated_input);
    printf("size: (%i, %i)\n", width, height);

    // If simulated input, spin off the simulator thread
    pthread_t tid;
    if (simulated_input)
    {
        if (pthread_create(&tid, NULL, &simulated_input_thread, &width) != 0)
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

    size_t buffer_size = width;
    float* buffer = (float*)calloc(sizeof(float), buffer_size);

    // Now set up our GUI
    InitWindow(width, height, "Raster");
    Raster raster = new_raster(width, height);
    SetTargetFPS(60);

    RenderTexture2D rtex = LoadRenderTexture(width, height);
    Color* line_of_pixels = (Color*)calloc(sizeof(Color), raster.width);

    while (!WindowShouldClose())
    {
        // Update
        while (1)
        {
            // Receive all queued up data and push to Raster before rendering frame
            if (zmq_recv(subscriber, buffer, sizeof(float) * buffer_size, ZMQ_DONTWAIT) == -1)
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

            // TODO: Apply colormap here to go from float -> Color.
            for (int i = 0; i < raster.width; i++)
            {
                line_of_pixels[i] = (Color) { buffer[i], buffer[i], buffer[i], 255 };
            }

            push_line(line_of_pixels, &raster);
        }

        // Render all the data we have
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
    UnloadRenderTexture(rtex);
    CloseWindow();
    free(buffer);
    zmq_close(subscriber);
    zmq_ctx_destroy(context);
    unlink("/dev/shm/ipc");

    return 0;
}
