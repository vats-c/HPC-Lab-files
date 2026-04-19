#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "init.h"

// Global variables
int GRID_X, GRID_Y, NX, NY;
int NUM_Points, Maxiter;
double dx, dy;
double min_val, max_val;

// Serial interpolation — scatter particles to mesh
static void serial_interpolation(double *mesh_value, Points *points) {
    memset(mesh_value, 0, GRID_X * GRID_Y * sizeof(double));

    for (int p = 0; p < NUM_Points; p++) {
        if (points[p].is_void) continue;

        int i = (int)(points[p].x / dx);
        int j = (int)(points[p].y / dy);

        if (i >= NX) i = NX - 1;
        if (j >= NY) j = NY - 1;
        if (i < 0) i = 0;
        if (j < 0) j = 0;

        double lx = points[p].x - i * dx;
        double ly = points[p].y - j * dy;

        double dx_lx = dx - lx;
        double dy_ly = dy - ly;

        int idx = j * GRID_X + i;

        mesh_value[idx]             += dx_lx * dy_ly;
        mesh_value[idx + 1]         += lx    * dy_ly;
        mesh_value[idx + GRID_X]    += dx_lx * ly;
        mesh_value[idx + GRID_X + 1] += lx   * ly;
    }
}

// Normalize mesh to [-1, 1]
static void serial_normalization(double *mesh_value) {
    int G = GRID_X * GRID_Y;
    min_val = mesh_value[0];
    max_val = mesh_value[0];

    for (int k = 0; k < G; k++) {
        if (mesh_value[k] < min_val) min_val = mesh_value[k];
        if (mesh_value[k] > max_val) max_val = mesh_value[k];
    }

    double range = max_val - min_val;
    if (range > 0.0) {
        for (int k = 0; k < G; k++) {
            mesh_value[k] = 2.0 * (mesh_value[k] - min_val) / range - 1.0;
        }
    }
}

// Reverse-interpolation: gather grid → particles, update positions
static void serial_mover(double *mesh_value, Points *points) {
    for (int p = 0; p < NUM_Points; p++) {
        if (points[p].is_void) continue;

        int i = (int)(points[p].x / dx);
        int j = (int)(points[p].y / dy);

        if (i >= NX) i = NX - 1;
        if (j >= NY) j = NY - 1;
        if (i < 0) i = 0;
        if (j < 0) j = 0;

        double lx = points[p].x - i * dx;
        double ly = points[p].y - j * dy;

        double dx_lx = dx - lx;
        double dy_ly = dy - ly;

        int idx = j * GRID_X + i;

        double Fi = dx_lx * dy_ly * mesh_value[idx]
                   + lx    * dy_ly * mesh_value[idx + 1]
                   + dx_lx * ly    * mesh_value[idx + GRID_X]
                   + lx    * ly    * mesh_value[idx + GRID_X + 1];

        points[p].x += Fi * dx;
        points[p].y += Fi * dy;

        if (points[p].x < 0.0 || points[p].x > 1.0 ||
            points[p].y < 0.0 || points[p].y > 1.0) {
            points[p].is_void = true;
        }
    }
}

// Denormalize mesh back to original range
static void serial_denormalization(double *mesh_value) {
    int G = GRID_X * GRID_Y;
    double range = max_val - min_val;

    if (range > 0.0) {
        for (int k = 0; k < G; k++) {
            mesh_value[k] = (mesh_value[k] + 1.0) * range / 2.0 + min_val;
        }
    }
}

// Count void particles
static long long int serial_void_count(Points *points) {
    long long int voids = 0;
    for (int i = 0; i < NUM_Points; i++) {
        voids += (int)points[i].is_void;
    }
    return voids;
}

// Write mesh to file
static void serial_save_mesh(double *mesh_value) {
    FILE *fd = fopen("Mesh.out", "w");
    if (!fd) { printf("Error creating Mesh.out\n"); exit(1); }

    for (int i = 0; i < GRID_Y; i++) {
        for (int j = 0; j < GRID_X; j++) {
            fprintf(fd, "%lf ", mesh_value[i * GRID_X + j]);
        }
        fprintf(fd, "\n");
    }
    fclose(fd);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) { printf("Error opening input file\n"); exit(1); }

    fread(&NX, sizeof(int), 1, file);
    fread(&NY, sizeof(int), 1, file);
    fread(&NUM_Points, sizeof(int), 1, file);
    fread(&Maxiter, sizeof(int), 1, file);

    GRID_X = NX + 1;
    GRID_Y = NY + 1;
    dx = 1.0 / NX;
    dy = 1.0 / NY;

    double *mesh_value = (double *)calloc(GRID_X * GRID_Y, sizeof(double));
    Points *points     = (Points *)calloc(NUM_Points, sizeof(Points));

    double total_int_time = 0.0, total_norm_time = 0.0;
    double total_move_time = 0.0, total_denorm_time = 0.0;

    read_points(file, points);

    for (int iter = 0; iter < Maxiter; iter++) {
        clock_t t0 = clock();
        serial_interpolation(mesh_value, points);
        clock_t t1 = clock();
        serial_normalization(mesh_value);
        clock_t t2 = clock();
        serial_mover(mesh_value, points);
        clock_t t3 = clock();
        serial_denormalization(mesh_value);
        clock_t t4 = clock();

        total_int_time    += (double)(t1 - t0) / CLOCKS_PER_SEC;
        total_norm_time   += (double)(t2 - t1) / CLOCKS_PER_SEC;
        total_move_time   += (double)(t3 - t2) / CLOCKS_PER_SEC;
        total_denorm_time += (double)(t4 - t3) / CLOCKS_PER_SEC;
    }

    serial_save_mesh(mesh_value);
    printf("Total Interpolation Time = %lf seconds\n", total_int_time);
    printf("Total Normalization Time = %lf seconds\n", total_norm_time);
    printf("Total Mover Time = %lf seconds\n", total_move_time);
    printf("Total Denormalization Time = %lf seconds\n", total_denorm_time);
    printf("Total Algorithm Time = %lf seconds\n",
           total_int_time + total_norm_time + total_move_time + total_denorm_time);
    printf("Total Number of Voids = %lld\n", serial_void_count(points));

    free(mesh_value);
    free(points);
    fclose(file);
    return 0;
}
