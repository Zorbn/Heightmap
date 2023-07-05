#include <MiniFB.h>

#include <inttypes.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define WIDTH 800
#define HEIGHT 600

const float FOV = (float)M_PI / 3.0f;
const float RAY_COUNT = WIDTH;
const float RAY_DISTANCE = 2000.0f;
const float SCALE_HEIGHT = 620.0f;

struct heightmap_state {
    uint8_t *color_map;
    uint8_t *height_map;
    int image_width;
    int image_height;
    float camera_angle;
    float camera_x;
    float camera_y;
    float camera_z;
    float camera_pitch;
};

void render_frame(uint32_t *buffer, int32_t *y_buffer, struct heightmap_state *heightmap_state) {
    memset(buffer, 0, sizeof(buffer));
    for (size_t i = 0; i < WIDTH; i++) {
        y_buffer[i] = HEIGHT;
    }

    const float ray_angle_delta = FOV / RAY_COUNT;
    const float half_fov = FOV * 0.5f;

    float ray_angle = heightmap_state->camera_angle - half_fov;
    for (size_t ray_i = 0; ray_i < WIDTH; ray_i++) {
        float angle_sin = sinf(ray_angle);
        float angle_cos = cosf(ray_angle);
        bool first_contact = false;

        for (int32_t depth = 1; depth < RAY_DISTANCE; depth++) {
            int32_t x = (int32_t)(heightmap_state->camera_x + depth * angle_cos);
            if (x < 0 || x >= heightmap_state->image_width) {
                continue;
            }

            int32_t z = (int32_t)(heightmap_state->camera_z + depth * angle_sin);
            if (z < 0 || z >= heightmap_state->image_height) {
                continue;
            }

            // Correct the "fish-eye" effect
            float corrected_depth = depth * cosf(heightmap_state->camera_angle - ray_angle);

            size_t pixel_i = (x + z * heightmap_state->image_width) * 3;
            uint32_t pixel_color = (heightmap_state->color_map[pixel_i] << 16) | (heightmap_state->color_map[pixel_i + 1] << 8) | heightmap_state->color_map[pixel_i + 2];
            uint8_t pixel_height = heightmap_state->height_map[pixel_i];

            int32_t height_on_screen =
                (int32_t)((heightmap_state->camera_y - pixel_height) / corrected_depth * SCALE_HEIGHT +
                          heightmap_state->camera_pitch);

            // Don't fill pixels under the map.
            if (!first_contact) {
                y_buffer[ray_i] = min(height_on_screen, HEIGHT);
                first_contact = true;
            }

            height_on_screen = max(height_on_screen, 0);

            if (height_on_screen < y_buffer[ray_i]) {
                for (int32_t screen_y = height_on_screen; screen_y < y_buffer[ray_i]; screen_y++) {
                    buffer[ray_i + screen_y * WIDTH] = pixel_color;
                }
                y_buffer[ray_i] = height_on_screen;
            }
        }

        ray_angle += ray_angle_delta;
    }
}

int main(void) {
    struct mfb_window *window = mfb_open_ex("Heightmap", WIDTH, HEIGHT, WF_RESIZABLE);
    if (!window)
        return 0;

    uint32_t *buffer = calloc(WIDTH * HEIGHT, sizeof(uint32_t));
    int32_t *y_buffer = calloc(WIDTH, sizeof(int32_t));
    struct mfb_timer *timer = mfb_timer_create();
    mfb_timer_reset(timer);

    int color_map_width, color_map_height, color_map_channels;
    uint8_t *color_map = stbi_load(
        "color_map.png", &color_map_width, &color_map_height, &color_map_channels, STBI_rgb);
    if (!color_map) {
        puts("Failed to load color map image!");
        return -1;
    }
    int height_map_width, height_map_height, height_map_channels;
    uint8_t *height_map = stbi_load(
        "height_map.png", &height_map_width, &height_map_height, &height_map_channels, STBI_rgb);
    if (!height_map) {
        puts("Failed to load height map image!");
        return -1;
    }

    struct heightmap_state heightmap_state = (struct heightmap_state){
        .color_map = color_map,
        .height_map = height_map,
        .image_width = min(color_map_width, height_map_width),
        .image_height = min(color_map_height, height_map_height),
        .camera_angle = (float)M_PI * 0.25f,
        .camera_x = -100.0f,
        .camera_y = 270.0f,
        .camera_z = 0.0f,
        .camera_pitch = 40.0f,
    };

    do {
        float delta_time = (float)mfb_timer_delta(timer);
        printf("frame time: %fs\n", delta_time);

        render_frame(buffer, y_buffer, &heightmap_state);

        int state = mfb_update_ex(window, buffer, WIDTH, HEIGHT);

        if (state < 0) {
            window = NULL;
            break;
        }
    } while (mfb_wait_sync(window));

    mfb_timer_destroy(timer);

    stbi_image_free(color_map);
    stbi_image_free(height_map);
}
