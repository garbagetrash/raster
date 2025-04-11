#include <errno.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

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


typedef struct {
  uint64_t num_frames;
  uint32_t frame_size;
  uint32_t bytes_per_element;
  uint64_t read_cntr;
  uint64_t write_cntr;
  uint64_t frame_cntr; // points to element within a frame
  bool hit_eof;
  char* buffer;
} ReadBuffer;

ReadBuffer* read_buffer = NULL;

ReadBuffer* new_read_buffer(uint32_t frame_size, uint64_t num_frames)
{
    ReadBuffer* output = (ReadBuffer*)malloc(sizeof(ReadBuffer));
    output->frame_size = frame_size;
    output->num_frames = num_frames;
    output->bytes_per_element = sizeof(float);
    output->write_cntr = 0;
    output->read_cntr = 0;
    output->frame_cntr = 0;
    output->hit_eof = false;
    output->buffer = (char*)malloc(num_frames * frame_size * output->bytes_per_element);
    return output;
}

void free_read_buffer(ReadBuffer* rb)
{
    if (rb != NULL)
    {
        free(rb->buffer);
    }
    free(rb);
}

void write_to_buffer(const char* data, size_t nbytes, ReadBuffer* rb)
{
    // bytes until end of read buffer
    size_t read_buffer_size = rb->num_frames * rb->frame_size * rb->bytes_per_element;
    ssize_t bytes_left = rb->frame_size * (rb->num_frames - rb->write_cntr % rb->num_frames) * rb->bytes_per_element;
    bytes_left -= rb->frame_cntr * rb->bytes_per_element;

    char* ptr = &(rb->buffer[((rb->write_cntr%rb->num_frames)*rb->frame_size+rb->frame_cntr)*rb->bytes_per_element]);
    if (nbytes >= read_buffer_size) {
        // If more than what fits in the entire read buffer, then just grab
        // what we can.
        memcpy(rb->buffer, &(data[nbytes - read_buffer_size]), read_buffer_size);
    } else if (nbytes > bytes_left) {
        // Will roll over the read buffer
        memcpy(ptr, data, bytes_left);
        memcpy(rb->buffer, &(data[bytes_left]), (ssize_t)nbytes - bytes_left);
    } else {
        // Fits in current buffer without roll over
        memcpy(ptr, data, nbytes);
    }

    uint64_t num_new_frames = nbytes / (rb->bytes_per_element * rb->frame_size);
    rb->frame_cntr += nbytes % (rb->bytes_per_element * rb->frame_size);
    if (rb->frame_cntr > rb->num_frames)
    {
        num_new_frames += rb->frame_cntr / rb->frame_size;
        rb->frame_cntr %= rb->frame_size;
    }
    rb->write_cntr += num_new_frames;

    // Force update read pointer if it's falling behind
    if (rb->read_cntr < rb->write_cntr - rb->num_frames)
    {
        rb->read_cntr = rb->write_cntr - rb->num_frames;
    }
}

// Read all complete frames from buffer in one go
void read_all_frames_from_buffer(ReadBuffer* rb, char* output, size_t* nbytes)
{
    size_t nframes = rb->write_cntr - rb->read_cntr;
    *nbytes = nframes * rb->frame_size * rb->bytes_per_element;
    if (nbytes > 0) {
        ssize_t fidx = rb->read_cntr % rb->num_frames;
        ssize_t bytes_left = (rb->num_frames - fidx) * rb->frame_size * rb->bytes_per_element;
        if (bytes_left < *nbytes)
        {
            memcpy(output, (void*)&(rb->buffer[fidx * rb->frame_size * rb->bytes_per_element]), bytes_left);
            memcpy(&(output[bytes_left]), (void*)rb->buffer, *nbytes - bytes_left);
        } else {
            // Not split across read_buffer
            memcpy(output, (void*)&(rb->buffer[fidx * rb->frame_size * rb->bytes_per_element]), *nbytes);
        }
        rb->read_cntr += nframes;
    }
}

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
        write_to_buffer(tmp, nbytes, read_buffer);
    }
    free(tmp);

    return NULL;
}


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
            idx = (y * width + x) % buffer_size;
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
    int frame_size = 1024;
    int c;
    float* colormap = (float*)inferno_srgb_floats;
    char* color_choice = NULL;

    while ((c = getopt(argc, argv, "f:c:")) != -1)
    {
        switch (c)
        {
            case 'f':
                frame_size = atoi(optarg);
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
    printf("colormap choice: %s\n", color_choice);

    read_buffer = new_read_buffer(frame_size, 16);

    // Now set up our GUI
    InitWindow(screen.width, screen.height, "Waterfall");
    screen.width = GetScreenWidth();
    screen.height = GetScreenHeight();
    Waterfall waterfall = new_waterfall(frame_size, screen.height);
    SetTargetFPS(60);
    Font font = LoadFont("resources/fonts/pixelplay.png");

    float* buffer = (float*)calloc(sizeof(float), 16 * frame_size);

    RenderTexture2D rtex = LoadRenderTexture(frame_size, screen.height);
    Color* line_of_pixels = (Color*)calloc(sizeof(Color), waterfall.width);
    Vector2 click_start = { 0, 0 };
    Vector2 click_end = { 0, 0 };
    int last_mouse = 0; // 0 - not pressed, 1 - pressed

    pthread_t tid;
    if (pthread_create(&tid, NULL, &spin_read, NULL) != 0)
    {
        perror("pthread_create");
        return -1;
    }

    Vector2 origin = { 0.0f, 0.0f };
    while (!WindowShouldClose())
    {
        // Update
        if (IsWindowResized())
        {
            // Window resized, so reset width/height
            screen.width = GetScreenWidth();
            screen.height = GetScreenHeight();

            free_waterfall(&waterfall);
            waterfall = new_waterfall(frame_size, screen.height);

            rtex = LoadRenderTexture(frame_size, screen.height);

            free(line_of_pixels);
            line_of_pixels = (Color*)calloc(sizeof(Color), waterfall.width);

            free(buffer);
            buffer = (float*)calloc(sizeof(float), 16 * frame_size);
        }

        size_t nbytes = 0;
        read_all_frames_from_buffer(read_buffer, (char*)buffer, &nbytes);
        size_t nframes = nbytes / (read_buffer->frame_size * read_buffer->bytes_per_element);

        for (size_t i = 0; i < nframes; i++)
        {
            // This also applies colormap
            //printf("%f\n", buffer[i*read_buffer->frame_size]);
            push_line(&(buffer[i*read_buffer->frame_size]), &waterfall, colormap);
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
            if (screen.zlevel > 0)
            {
                // Some zoom to be applied
                Zoom z0 = screen.zoom_stack[0];
                Zoom z1 = screen.zoom_stack[screen.zlevel];
                Vector2 xy = to_pixels((Vector2){ z1.logical_minx, z1.logical_miny }, &screen);
                float wratio = z1.logical_width / z0.logical_width;
                float hratio = z1.logical_height / z0.logical_height;
                Rectangle texture_patch = { xy.x, xy.y, screen.width * wratio, screen.height * hratio };
                NPatchInfo patch_info = {
                    texture_patch, 0, 0, 0, 0, NPATCH_NINE_PATCH
                };
                DrawTextureNPatch(
                        rtex.texture,
                        patch_info,
                        (Rectangle) { 0.0f, 0.0f, screen.width, screen.height },
                        origin,
                        0.0f,
                        WHITE
                );
                float y = waterfall.yidx / hratio;
                DrawLine(0, y, screen.width, y, YELLOW);
            } else {
                DrawTexture(rtex.texture, 0, 0, WHITE);
                DrawLine(0, waterfall.yidx, screen.width, waterfall.yidx, YELLOW);
            }

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
    free(line_of_pixels);
    free_waterfall(&waterfall);
    UnloadRenderTexture(rtex);
    CloseWindow();
    free(buffer);
    UnloadFont(font);
    free_read_buffer(read_buffer);

    return 0;
}
