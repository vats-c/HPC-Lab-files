#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

typedef struct {
    double x, y;
    bool is_void;
} Point;

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        printf("Error opening input file\n");
        return 1;
    }

    int NX, NY, NUM_Points, Maxiter;
    fread(&NX, sizeof(int), 1, file);
    fread(&NY, sizeof(int), 1, file);
    fread(&NUM_Points, sizeof(int), 1, file);
    fread(&Maxiter, sizeof(int), 1, file);

    int GRID_X = NX + 1;
    int GRID_Y = NY + 1;
    double dx = 1.0 / NX;
    double dy = 1.0 / NY;
    double inv_dx = NX;
    double inv_dy = NY;

    double *mesh_value = (double *)calloc(GRID_X * GRID_Y, sizeof(double));
    Point *points = (Point *)malloc(NUM_Points * sizeof(Point));

    for (int i = 0; i < NUM_Points; i++) {
        fread(&points[i].x, sizeof(double), 1, file);
        fread(&points[i].y, sizeof(double), 1, file);
        points[i].is_void = false;
    }
    fclose(file);

    clock_t total_start = clock();

    for (int iter = 0; iter < Maxiter; iter++) {
        // Zero mesh
        memset(mesh_value, 0, GRID_X * GRID_Y * sizeof(double));

        // 1. Interpolation
        for (int i = 0; i < NUM_Points; i++) {
            if (points[i].is_void) continue;

            double px = points[i].x;
            double py = points[i].y;

            int i_idx = (int)(px * inv_dx);
            int j_idx = (int)(py * inv_dy);

            if (i_idx < 0) i_idx = 0; else if (i_idx >= NX) i_idx = NX - 1;
            if (j_idx < 0) j_idx = 0; else if (j_idx >= NY) j_idx = NY - 1;

            double X_i = i_idx * dx;
            double Y_j = j_idx * dy;

            double lx = px - X_i;
            double ly = py - Y_j;

            double w00 = (dx - lx) * (dy - ly);
            double w10 = lx * (dy - ly);
            double w01 = (dx - lx) * ly;
            double w11 = lx * ly;

            mesh_value[j_idx * GRID_X + i_idx] += w00; // f_i = 1
            mesh_value[j_idx * GRID_X + (i_idx + 1)] += w10;
            mesh_value[(j_idx + 1) * GRID_X + i_idx] += w01;
            mesh_value[(j_idx + 1) * GRID_X + (i_idx + 1)] += w11;
        }

        // 2. Normalization
        double min_val = mesh_value[0];
        double max_val = mesh_value[0];
        for (int i = 1; i < GRID_X * GRID_Y; i++) {
            if (mesh_value[i] < min_val) min_val = mesh_value[i];
            if (mesh_value[i] > max_val) max_val = mesh_value[i];
        }

        double range = max_val - min_val;
        if (range > 1e-12) {
            for (int i = 0; i < GRID_X * GRID_Y; i++) {
                mesh_value[i] = 2.0 * (mesh_value[i] - min_val) / range - 1.0;
            }
        } else {
            for (int i = 0; i < GRID_X * GRID_Y; i++) {
                mesh_value[i] = 0.0;
            }
        }

        // 3. Mover (Reverse Interpolation)
        for (int i = 0; i < NUM_Points; i++) {
            if (points[i].is_void) continue;

            double px = points[i].x;
            double py = points[i].y;

            int i_idx = (int)(px * inv_dx);
            int j_idx = (int)(py * inv_dy);

            if (i_idx < 0) i_idx = 0; else if (i_idx >= NX) i_idx = NX - 1;
            if (j_idx < 0) j_idx = 0; else if (j_idx >= NY) j_idx = NY - 1;

            double X_i = i_idx * dx;
            double Y_j = j_idx * dy;

            double lx = px - X_i;
            double ly = py - Y_j;

            double w00 = (dx - lx) * (dy - ly);
            double w10 = lx * (dy - ly);
            double w01 = (dx - lx) * ly;
            double w11 = lx * ly;

            double F_i = w00 * mesh_value[j_idx * GRID_X + i_idx] +
                         w10 * mesh_value[j_idx * GRID_X + (i_idx + 1)] +
                         w01 * mesh_value[(j_idx + 1) * GRID_X + i_idx] +
                         w11 * mesh_value[(j_idx + 1) * GRID_X + (i_idx + 1)];

            points[i].x += F_i * dx;
            points[i].y += F_i * dy;

            if (points[i].x < 0.0 || points[i].x > 1.0 || points[i].y < 0.0 || points[i].y > 1.0) {
                points[i].is_void = true;
            }
        }

        // 4. Denormalization
        if (range > 1e-12) {
            for (int i = 0; i < GRID_X * GRID_Y; i++) {
                mesh_value[i] = (mesh_value[i] + 1.0) / 2.0 * range + min_val;
            }
        } else {
            for (int i = 0; i < GRID_X * GRID_Y; i++) {
                mesh_value[i] = min_val;
            }
        }
    }

    clock_t total_end = clock();
    double total_time = (double)(total_end - total_start) / CLOCKS_PER_SEC;

    int active_count = 0;
    for (int i = 0; i < NUM_Points; i++) {
        if (!points[i].is_void) active_count++;
    }

    FILE *fd = fopen("Mesh.out", "w");
    if (fd) {
        for (int i = 0; i < GRID_Y; i++) {
            for (int j = 0; j < GRID_X; j++) {
                fprintf(fd, "%lf ", mesh_value[i * GRID_X + j]);
            }
            fprintf(fd, "\n");
        }
        fclose(fd);
    }

    printf("Serial baseline finished.\n");
    printf("Total time: %f seconds\n", total_time);
    printf("Active points remaining: %d / %d\n", active_count, NUM_Points);

    free(mesh_value);
    free(points);
    return 0;
}
