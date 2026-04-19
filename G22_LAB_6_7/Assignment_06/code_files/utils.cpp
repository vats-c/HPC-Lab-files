#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "utils.h"

// Optimized parallel interpolation using thread-local grids + parallel reduction
void interpolation(double *mesh_value, Points *points) {
    const int G_SIZE = GRID_X * GRID_Y;
    const double inv_dx = 1.0 / dx;
    const double inv_dy = 1.0 / dy;
    const int gx = GRID_X;
    const int nx = NX;
    const int ny = NY;
    const double ddx = dx;
    const double ddy = dy;
    const int npts = NUM_Points;

    // Persistent thread-local storage to avoid malloc/free overhead across iterations
    static double **all_local = NULL;
    static int cached_nthreads = 0;
    static int cached_gsize = 0;

    int nthreads;

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();

        // First thread sets up shared storage (only once or if config changes)
        #pragma omp single
        {
            nthreads = omp_get_num_threads();
            if (all_local == NULL || nthreads != cached_nthreads || G_SIZE != cached_gsize) {
                if (all_local) {
                    for (int t = 0; t < cached_nthreads; t++) free(all_local[t]);
                    free(all_local);
                }
                all_local = (double **)malloc(nthreads * sizeof(double *));
                for (int t = 0; t < nthreads; t++) {
                    all_local[t] = (double *)malloc(G_SIZE * sizeof(double));
                }
                cached_nthreads = nthreads;
                cached_gsize = G_SIZE;
            }
        }

        // Zero out this thread's local grid
        memset(all_local[tid], 0, G_SIZE * sizeof(double));
        double *my_mesh = all_local[tid];

        // Each thread interpolates its chunk of points into its own local grid
        // No race conditions - each thread writes to its own array
        #pragma omp for schedule(static)
        for (int p = 0; p < npts; p++) {
            // Use multiplication instead of division for cell index
            int i = (int)(points[p].x * inv_dx);
            int j = (int)(points[p].y * inv_dy);

            // Boundary clamping
            if (i >= nx) i = nx - 1;
            if (j >= ny) j = ny - 1;

            double lx = points[p].x - i * ddx;
            double ly = points[p].y - j * ddy;

            double dx_lx = ddx - lx;
            double dy_ly = ddy - ly;

            int idx = j * gx + i;

            my_mesh[idx]         += dx_lx * dy_ly;  // w(i,j)
            my_mesh[idx + 1]     += lx * dy_ly;     // w(i+1,j)
            my_mesh[idx + gx]    += dx_lx * ly;     // w(i,j+1)
            my_mesh[idx + gx + 1] += lx * ly;       // w(i+1,j+1)
        }

        // Parallel reduction: each thread reduces a slice of the grid
        // Much faster than #pragma omp critical which serializes everything
        #pragma omp for schedule(static)
        for (int k = 0; k < G_SIZE; k++) {
            for (int t = 0; t < nthreads; t++) {
                mesh_value[k] += all_local[t][k];
            }
        }
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