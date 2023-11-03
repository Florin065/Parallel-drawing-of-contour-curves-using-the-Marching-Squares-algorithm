#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#include "helpers.h"

#define CLAMP(v, min, max) if (v < min) { v = min; } else if (v > max) { v = max; }

#define BARRIER_RESCALE 0
#define BARRIER_GSAMPLE 1
#define NUM_BARRIERS 2

typedef struct {
    ppm_image          *input;                  // input image
    ppm_image          *image;                  // scaled image
    ppm_image         **map;                    // contour map
    unsigned char     **grid;                   // grid
    int                 num_threads;            // number of threads
    pthread_barrier_t   barriers[NUM_BARRIERS]; // barriers
} global_t;

typedef struct {
    global_t           *global;                 // global data
    int                 tid;                    // thread id
} thread_payload_t;

void rescale_image(ppm_image *input, ppm_image *image,
                   int num_threads, int tid) {
    uint8_t sample[3];

    int start = tid * RESCALE_X / num_threads;
    int end   = fmin((tid + 1) * RESCALE_X / num_threads, RESCALE_X);

    for (int i = start; i < end; i++) {
        for (int j = 0; j < image->y; j++) {
            float u = (float)i / (float)(image->x - 1);
            float v = (float)j / (float)(image->y - 1);
            sample_bicubic(input, u, v, sample);

            image->data[i * image->y + j].red = sample[0];
            image->data[i * image->y + j].green = sample[1];
            image->data[i * image->y + j].blue = sample[2];
        }
    }
}

void sample_grid(unsigned char **grid, ppm_image *image,
                 int num_threads, int tid) {
    int p     = image->x / STEP;
    int q     = image->y / STEP;

    int start = tid * p / num_threads;
    int end   = fmin((tid + 1) * p / num_threads, p);

    ppm_pixel     curr_pix;
    unsigned char curr_col;

    for (int i = start; i < end; i++) {
        grid[i] = malloc((q + 1) * sizeof(unsigned char));

        for (int j = 0; j < q; j++) {
            curr_pix = image->data[i * STEP * image->y + j * STEP];
            curr_col = (curr_pix.red + curr_pix.green + curr_pix.blue) / 3;
            
            if (curr_col > SIGMA) {
                grid[i][j] = 0;
            } else {
                grid[i][j] = 1;
            }
        }
    }

    for (int i = start; i < end; i++) {
        curr_pix = image->data[i * STEP * image->y + image->x - 1];
        curr_col = (curr_pix.red + curr_pix.green + curr_pix.blue) / 3;
        
        if (curr_col > SIGMA) {
            grid[i][q] = 0;
        } else {
            grid[i][q] = 1;
        }
    }

    if (tid == 0) {
        grid[p] = malloc((q + 1) * sizeof(unsigned char));

        for (int j = 0; j < q; j++) {
            curr_pix = image->data[(image->x - 1) * image->y + j * STEP];
            curr_col = (curr_pix.red + curr_pix.green + curr_pix.blue) / 3;

            if (curr_col > SIGMA) {
                grid[p][j] = 0;
            } else {
                grid[p][j] = 1;
            }
        }
    }
}

void march(ppm_image *image, unsigned char **grid, ppm_image **map,
           int num_threads, int tid) {
    int p     = image->x / STEP;
    int q     = image->y / STEP;
    int start = tid * p / num_threads;
    int end   = fmin((tid + 1) * p / num_threads, p);

    for (int i = start; i < end; i++) {
        for (int j = 0; j < q; j++) {
            unsigned char k = 8 * grid[i][j] + 4 * grid[i][j + 1]
                            + 2 * grid[i + 1][j + 1] + grid[i + 1][j];
            int x = i * STEP;
            int y = j * STEP;

            for (int a = 0; a < map[k]->x; a++) {
                for (int b = 0; b < map[k]->y; b++) {
                    int contour_pixel_index = map[k]->x * a + b;
                    int image_pixel_index = (x + a) * image->y + y + b;

                    image->data[image_pixel_index].red
                                    = map[k]->data[contour_pixel_index].red;
                    image->data[image_pixel_index].green
                                    = map[k]->data[contour_pixel_index].green;
                    image->data[image_pixel_index].blue
                                    = map[k]->data[contour_pixel_index].blue;
                }
            }
        }
    }
}

void *thread_function(void *args) {
    global_t *global = ((thread_payload_t *) args)->global;
    int tid          = ((thread_payload_t *) args)->tid;

    if (global->input->x > RESCALE_X || global->input->y > RESCALE_Y) {
        rescale_image(global->input, global->image, global->num_threads, tid);
    }

    pthread_barrier_wait(&global->barriers[BARRIER_RESCALE]);

    sample_grid(global->grid, global->image, global->num_threads, tid);

    pthread_barrier_wait(&global->barriers[BARRIER_GSAMPLE]);

    march(global->image, global->grid, global->map, global->num_threads, tid);

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file> <num_threads>\n",
                                                                      argv[0]);
        exit(1);
    }

    global_t global = {
        .input       = read_ppm(argv[1]),
        .num_threads = atoi(argv[3]),
    };

    if (global.input->x > RESCALE_X || global.input->y > RESCALE_Y) {
        global.image       = malloc(sizeof(ppm_image));
        global.image->data = malloc(RESCALE_X * RESCALE_Y * sizeof(ppm_pixel));
        global.image->x    = RESCALE_X;
        global.image->y    = RESCALE_Y;
    } else {
        global.image       = global.input;
    }

    global.grid
            = malloc((global.image->x / STEP + 1) * sizeof(unsigned char *));
    global.map
            = malloc(CONTOUR_CONFIG_COUNT * sizeof(ppm_image *));

    for (int i = 0; i < CONTOUR_CONFIG_COUNT; i++) {
        char filename[FILENAME_MAX_SIZE];
        sprintf(filename, "./contours/%d.ppm", i);
        global.map[i] = read_ppm(filename);
    }

    for (int i = 0; i < NUM_BARRIERS; i++) {
        if (pthread_barrier_init(&global.barriers[i], NULL, global.num_threads))
            exit(1);
    }

    thread_payload_t payloads[global.num_threads];
    pthread_t threads[global.num_threads];

    for (int i = 0; i < global.num_threads; i++) {
        payloads[i] = (thread_payload_t) {
            .global = &global,
            .tid    = i
        };

        if (pthread_create(&threads[i], NULL, thread_function, &payloads[i]))
            exit(1);
    }

    for (int i = 0; i < global.num_threads; i++) {
        if (pthread_join(threads[i], NULL))
            exit(1);
    }

    for (int i = 0; i < NUM_BARRIERS; i++) {
        if (pthread_barrier_destroy(&global.barriers[i]))
            exit(1);
    }

    write_ppm(global.image, argv[2]);

    exit(0);
}
