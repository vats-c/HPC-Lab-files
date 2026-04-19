#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "init.h"

// Forward declarations
void serial_save_mesh(double *mesh_value);

// Global variables
int GRID_X, GRID_Y, NX, NY;
int NUM_Points, Maxiter;
double dx, dy;

// Serial interpolation (baseline for timing comparison)
void serial_interpolation(double *mesh_value, Points *points) {
    for (int p = 0; p < NUM_Points; p++) {
        int i = (int)(points[p].x / dx);
        int j = (int)(points[p].y / dy);

        if (i >= NX) i = NX - 1;
        if (j >= NY) j = NY - 1;
        if (i < 0) i = 0;
        if (j < 0) j = 0;

        double Xi = i * dx;
        double Yj = j * dy;
        double lx = points[p].x - Xi;
        double ly = points[p].y - Yj;

        double w00 = (dx - lx) * (dy - ly);
        double w10 = lx * (dy - ly);
        double w01 = (dx - lx) * ly;
        double w11 = lx * ly;

        mesh_value[j * GRID_X + i] += w00;
        mesh_value[j * GRID_X + (i + 1)] += w10;
        mesh_value[(j + 1) * GRID_X + i] += w01;
        mesh_value[(j + 1) * GRID_X + (i + 1)] += w11;
    }
}

int main(int argc, char **argv) {

    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        printf("Error opening input file\n");
        exit(1);
    }

    fread(&NX, sizeof(int), 1, file);
    fread(&NY, sizeof(int), 1, file);
    fread(&NUM_Points, sizeof(int), 1, file);
    fread(&Maxiter, sizeof(int), 1, file);

    GRID_X = NX + 1;
    GRID_Y = NY + 1;
    dx = 1.0 / NX;
    dy = 1.0 / NY;

    double *mesh_value = (double *) calloc(GRID_X * GRID_Y, sizeof(double));
    Points *points = (Points *) calloc(NUM_Points, sizeof(Points));

    double total_time = 0.0;

    for (int iter = 0; iter < Maxiter; iter++) {
        read_points(file, points);

        clock_t start = clock();
        serial_interpolation(mesh_value, points);
        clock_t end = clock();

        total_time += (double)(end - start) / CLOCKS_PER_SEC;
    }

    serial_save_mesh(mesh_value);
    printf("Total interpolation time (serial) = %lf seconds\n", total_time);

    free(mesh_value);
    free(points);
    fclose(file);

    return 0;
}

// Write mesh to file (embedded to avoid OpenMP dependency)
void serial_save_mesh(double *mesh_value) {
    FILE *fd = fopen("Mesh.out", "w");
    if (!fd) {
        printf("Error creating Mesh.out\n");
        exit(1);
    }
    for (int i = 0; i < GRID_Y; i++) {
        for (int j = 0; j < GRID_X; j++) {
            if (j == GRID_X - 1) {
                fprintf(fd, "%lf", mesh_value[i * GRID_X + j]);
            } else {
                fprintf(fd, "%lf ", mesh_value[i * GRID_X + j]);
            }
        }
        fprintf(fd, "\n");
    }
    fclose(fd);
}
