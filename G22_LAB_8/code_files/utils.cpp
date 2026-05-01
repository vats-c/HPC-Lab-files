#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>
#include <omp.h>
#include "utils.h"

// Hybrid MPI + OpenMP interpolation pipeline
void interpolation(double *local_mesh, double *global_mesh, Points *points, int local_num, double *timings) {
    
    int GRID_SIZE = GRID_X * GRID_Y;
    double inv_dx = NX;
    double inv_dy = NY;

    int max_threads = omp_get_max_threads();
    long thread_mem = (long)GRID_SIZE * max_threads * sizeof(double);
    bool use_atomics = (thread_mem > 20000000); // 20 MB cache limit

    double **thread_meshes = NULL;
    if (!use_atomics) {
        thread_meshes = (double **)malloc(max_threads * sizeof(double *));
        for (int i = 0; i < max_threads; i++) {
            thread_meshes[i] = (double *)malloc(GRID_SIZE * sizeof(double));
        }
    }

    double t0, t1;

    // 1. Interpolation
    t0 = MPI_Wtime();
    memset(local_mesh, 0, GRID_SIZE * sizeof(double));

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int num_threads = omp_get_num_threads();

        if (!use_atomics) {
            double *my_mesh = thread_meshes[tid];
            memset(my_mesh, 0, GRID_SIZE * sizeof(double));

            #pragma omp for schedule(static)
            for (int i = 0; i < local_num; i++) {
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

                my_mesh[j_idx * GRID_X + i_idx] += w00;
                my_mesh[j_idx * GRID_X + (i_idx + 1)] += w10;
                my_mesh[(j_idx + 1) * GRID_X + i_idx] += w01;
                my_mesh[(j_idx + 1) * GRID_X + (i_idx + 1)] += w11;
            }

            #pragma omp for schedule(static)
            for (int k = 0; k < GRID_SIZE; k++) {
                double sum = 0;
                for (int t = 0; t < num_threads; t++) {
                    sum += thread_meshes[t][k];
                }
                local_mesh[k] = sum;
            }
        } else {
            #pragma omp for schedule(static)
            for (int i = 0; i < local_num; i++) {
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

                #pragma omp atomic
                local_mesh[j_idx * GRID_X + i_idx] += w00;
                #pragma omp atomic
                local_mesh[j_idx * GRID_X + (i_idx + 1)] += w10;
                #pragma omp atomic
                local_mesh[(j_idx + 1) * GRID_X + i_idx] += w01;
                #pragma omp atomic
                local_mesh[(j_idx + 1) * GRID_X + (i_idx + 1)] += w11;
            }
        }
    }
    t1 = MPI_Wtime();
    timings[0] += (t1 - t0);

    // 2. MPI Communication
    t0 = MPI_Wtime();
    MPI_Allreduce(local_mesh, global_mesh, GRID_SIZE, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    t1 = MPI_Wtime();
    timings[1] += (t1 - t0);

    // 3. Normalization
    t0 = MPI_Wtime();
    double min_val = global_mesh[0];
    double max_val = global_mesh[0];
    
    #pragma omp parallel for reduction(min:min_val) reduction(max:max_val)
    for (int i = 1; i < GRID_SIZE; i++) {
        if (global_mesh[i] < min_val) min_val = global_mesh[i];
        if (global_mesh[i] > max_val) max_val = global_mesh[i];
    }

    double range = max_val - min_val;
    if (range > 1e-12) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < GRID_SIZE; i++) {
            global_mesh[i] = 2.0 * (global_mesh[i] - min_val) / range - 1.0;
        }
    } else {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < GRID_SIZE; i++) {
            global_mesh[i] = 0.0;
        }
    }
    t1 = MPI_Wtime();
    timings[2] += (t1 - t0);

    // 4. Mover
    t0 = MPI_Wtime();
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < local_num; i++) {
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

        double F_i = w00 * global_mesh[j_idx * GRID_X + i_idx] +
                     w10 * global_mesh[j_idx * GRID_X + (i_idx + 1)] +
                     w01 * global_mesh[(j_idx + 1) * GRID_X + i_idx] +
                     w11 * global_mesh[(j_idx + 1) * GRID_X + (i_idx + 1)];

        points[i].x += F_i * dx;
        points[i].y += F_i * dy;

        if (points[i].x < 0.0 || points[i].x > 1.0 || points[i].y < 0.0 || points[i].y > 1.0) {
            points[i].is_void = true;
        }
    }
    t1 = MPI_Wtime();
    timings[3] += (t1 - t0);

    // 5. Denormalization
    if (range > 1e-12) {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < GRID_SIZE; i++) {
            global_mesh[i] = (global_mesh[i] + 1.0) / 2.0 * range + min_val;
        }
    } else {
        #pragma omp parallel for schedule(static)
        for (int i = 0; i < GRID_SIZE; i++) {
            global_mesh[i] = min_val;
        }
    }

    if (!use_atomics && thread_meshes) {
        for (int i = 0; i < max_threads; i++) {
            free(thread_meshes[i]);
        }
        free(thread_meshes);
    }
}

// Write mesh to file
void save_mesh(double *mesh_value) {
    FILE *fd = fopen("Mesh.out", "w");
    if (!fd) {
        printf("Error creating Mesh.out\n");
        exit(1);
    }

    for (int i = 0; i < GRID_Y; i++) {
        for (int j = 0; j < GRID_X; j++) {
            fprintf(fd, "%lf ", mesh_value[i * GRID_X + j]);
        }
        fprintf(fd, "\n");
    }
    fclose(fd);
}