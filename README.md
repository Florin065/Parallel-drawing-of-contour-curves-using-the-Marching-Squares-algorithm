# Homework 1 APD - Marching Squares Algorithm with Multithreading

<div align="center"><img src="https://media.tenor.com/Zf45U-rHMgkAAAAC/friday-good-morning.gif" width="300px"></div>

## Homework Goals

* Starting from the sequential implementation, to parallelize, with the help of Pthreads in C/C++, a program that generates contours for topological maps using the Marching Squares algorithm. The final goal is to obtain the same output files as the sequential version, but with improved execution times.

## Overview

* This code is a C program implements Marching Squares Algorithm that performs contour mapping on an input image, using multithreading. Contour mapping is a technique that highlights edges and contours in an image. The code reads an input image, scales it if necessary, samples the image to create a grid, and then applies contour mapping using a set of predefined contour configurations. Multithreading is used to parallelize the processing for improved performance.

## Code Structure

### Structures

```c
typedef struct {
    unsigned char red, green, blue;             // RGB values
} ppm_pixel;
```

* represents a single pixel in a PPM image.
* fields:
    * **red**, **green**, **blue**: Red, green, and blue color channels of the pixel.

```c
typedef struct {
    int x, y;                                   // image width & height
    ppm_pixel *data;                            // pixel data
} ppm_image;
```

* represents a PPM (Portable Pixmap) image.
* fields:
    * **x**: Width of the image.
    * **y**: Height of the image.
    * **data**: An array of ppm_pixel elements representing the image pixels.

```c
typedef struct {
    ppm_image          *input;                  // input image
    ppm_image          *image;                  // scaled image
    ppm_image         **map;                    // contour map
    unsigned char     **grid;                   // grid
    int                 num_threads;            // number of threads
    pthread_barrier_t   barriers[NUM_BARRIERS]; // barriers
} global_t;
```

* holds global data used by multiple functions.
* fields:
    * **input**: Pointer to the input image.
    * **image**: Pointer to the scaled image.
    * **map**: An array of pointers to contour map images.
    * **grid**: 2D array representing a grid sampled from the image.
    * **num_threads**: Number of threads to use for parallel processing.
    * **barriers**: An array of pthread_barrier_t objects for synchronization.

```c
typedef struct {
    global_t           *global;                 // global data
    int                 tid;                    // thread id
} thread_payload_t;
```

* used to pass data to individual threads.
* fields:
    * **global**: Pointer to the global data.
    * **tid**: Thread ID.

### Functions

```c
void rescale_image(ppm_image *input, ppm_image *image, int num_threads, int tid);
```

* Scales the input image to a specified size using bicubic interpolation.
* By comparison with the sequential implementation, in the parallel one I parallelized the outer for.

```c
void sample_grid(unsigned char **grid, ppm_image *image, int num_threads, int tid);
```

* Samples the scaled image to create a grid of values based on color intensity.
* By comparison with the sequential implementation, in the parallel one I parallelized all the outer for and made an if for the case that the thread id (tid) is 0.

```c
void march(ppm_image *image, unsigned char **grid, ppm_image **map, int num_threads, int tid);
```

* Performs contour mapping by iterating over the grid and applying contour configurations to the image.
* By comparison with the sequential implementation, in the parallel one I moved the update_image function directly to the march function and for performance I parallelized the outer for.

```c
void *thread_function(void *args);
```

* Main function executed by each thread. It coordinates the processing steps and synchronization using barriers.
* The initial image is rescaled if necessary, after which all the threads are waited to finish the rescale execution, then it samples the rescaled image and waits for all the threads at the barrier, and finally the initial image is updated with the contour map pixels

```c
int main(int argc, char *argv[]);
```

* Main entry point of the program. It reads command line arguments, initializes data structures, creates threads, and coordinates the execution of the different processing steps. It also cleans up resources and writes the resulting image to an output file.
* Initialize global_t structure with the related values received from argv, after which if the image needs to be rescaled, new memory allocations are made, otherwise the same picture from the input remains, memory is allocated for the grid and contour map, the values are read for the contour map, the barriers are initialized and the necessary thread operations are performed, and at the end the barriers are destroyed and the image is written to the output file.

## Dependencies
* The code relies on the PPM image format and assumes that contour map images are stored in separate PPM files in a subdirectory called "contours."

## Usage

* To run the code, execute the program from the command line with the following arguments:
```bash
./tema1_par <input_file> <output_file> <num_threads>
```

## Note
* The code uses barriers for synchronization between threads to ensure proper coordination of tasks.
* The code resizes the input image if its dimensions exceed specified values, and this process is parallelized across threads for performance.
* The contour mapping process uses predefined contour configurations stored in separate PPM files.
