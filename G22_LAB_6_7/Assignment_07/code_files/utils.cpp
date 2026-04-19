#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "utils.h"

double min_val, max_val;

// ---------------------------------------------------------------------------
// HYBRID Parallel Interpolation
// Strategy 1 (small grids): Thread-local grids + parallel reduction
//   → Zero contention, but T×G memory + O(G×T) reduction
// Strategy 2 (large grids): Atomics on shared grid
//   → One grid fits in cache, low contention for large grids
// Switch point: when total thread-local memory would exceed L3 cache
// ---------------------------------------------------------------------------
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

    // Determine thread count
    int nthreads;
    #pragma omp parallel
    {
        #pragma omp single
        nthreads = omp_get_num_threads();
    }

    // Heuristic: if total thread-local memory > ~8 MB, atomics are faster
    // because one shared grid fits in L3, but T copies don't
    const long long total_local_bytes = (long long)G_SIZE * nthreads * sizeof(double);
    const long long CACHE_THRESHOLD = 20LL * 1024 * 1024; // 20 MB

    if (total_local_bytes > CACHE_THRESHOLD) {
        // ===== ATOMIC APPROACH (large grids) =====
        // Single shared grid stays in L3 cache
        // Low contention: particles/cell is small for large grids
        memset(mesh_value, 0, G_SIZE * sizeof(double));

        #pragma omp parallel for schedule(static)
        for (int p = 0; p < npts; p++) {
            if (points[p].is_void) continue;

            int i = (int)(points[p].x * inv_dx);
            int j = (int)(points[p].y * inv_dy);

            if (i >= nx) i = nx - 1;
            if (j >= ny) j = ny - 1;

            double lx = points[p].x - i * ddx;
            double ly = points[p].y - j * ddy;

            double dx_lx = ddx - lx;
            double dy_ly = ddy - ly;

            int idx = j * gx + i;

            #pragma omp atomic
            mesh_value[idx]         += dx_lx * dy_ly;
            #pragma omp atomic
            mesh_value[idx + 1]     += lx    * dy_ly;
            #pragma omp atomic
            mesh_value[idx + gx]    += dx_lx * ly;
            #pragma omp atomic
            mesh_value[idx + gx + 1] += lx   * ly;
        }
    } else {
        // ===== THREAD-LOCAL APPROACH (small/medium grids) =====
        // T copies fit in L3 → excellent cache performance
        // Zero contention during scatter, parallel reduction at end

        // Persistent thread-local storage — allocated once, reused
        static double **all_local = NULL;
        static int cached_nthreads = 0;
        static int cached_gsize = 0;

        #pragma omp parallel
        {
            int tid = omp_get_thread_num();

            #pragma omp single
            {
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

            // Zero this thread's local grid
            memset(all_local[tid], 0, G_SIZE * sizeof(double));
            double *my_mesh = all_local[tid];

            // Scatter particles — each thread writes to own grid
            #pragma omp for schedule(static)
            for (int p = 0; p < npts; p++) {
                if (points[p].is_void) continue;

                int i = (int)(points[p].x * inv_dx);
                int j = (int)(points[p].y * inv_dy);

                if (i >= nx) i = nx - 1;
                if (j >= ny) j = ny - 1;

                double lx = points[p].x - i * ddx;
                double ly = points[p].y - j * ddy;

                double dx_lx = ddx - lx;
                double dy_ly = ddy - ly;

                int idx = j * gx + i;

                my_mesh[idx]         += dx_lx * dy_ly;
                my_mesh[idx + 1]     += lx    * dy_ly;
                my_mesh[idx + gx]    += dx_lx * ly;
                my_mesh[idx + gx + 1] += lx   * ly;
            }

            // Parallel reduction — write to global mesh
            #pragma omp for schedule(static)
            for (int k = 0; k < G_SIZE; k++) {
                double sum = 0.0;
                for (int t = 0; t < nthreads; t++) {
                    sum += all_local[t][k];
                }
                mesh_value[k] = sum;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Normalization: scale mesh to [-1, 1]
// ---------------------------------------------------------------------------
void normalization(double *mesh_value) {
    const int G_SIZE = GRID_X * GRID_Y;
    double local_min = mesh_value[0];
    double local_max = mesh_value[0];

    #pragma omp parallel for reduction(min:local_min) reduction(max:local_max)
    for (int k = 0; k < G_SIZE; k++) {
        if (mesh_value[k] < local_min) local_min = mesh_value[k];
        if (mesh_value[k] > local_max) local_max = mesh_value[k];
    }

    min_val = local_min;
    max_val = local_max;

    double range = max_val - min_val;
    if (range > 0.0) {
        double inv_range = 2.0 / range;
        #pragma omp parallel for schedule(static)
        for (int k = 0; k < G_SIZE; k++) {
            mesh_value[k] = (mesh_value[k] - min_val) * inv_range - 1.0;
        }
    }
}

// ---------------------------------------------------------------------------
// Mover: gather grid → particles, update positions
// Embarrassingly parallel — no synchronization needed
// ---------------------------------------------------------------------------
void mover(double *mesh_value, Points *points) {
    const int npts = NUM_Points;
    const double inv_dx = 1.0 / dx;
    const double inv_dy = 1.0 / dy;
    const int gx = GRID_X;
    const int nx = NX;
    const int ny = NY;
    const double ddx = dx;
    const double ddy = dy;

    #pragma omp parallel for schedule(static)
    for (int p = 0; p < npts; p++) {
        if (points[p].is_void) continue;

        int i = (int)(points[p].x * inv_dx);
        int j = (int)(points[p].y * inv_dy);

        if (i >= nx) i = nx - 1;
        if (j >= ny) j = ny - 1;
        if (i < 0) i = 0;
        if (j < 0) j = 0;

        double lx = points[p].x - i * ddx;
        double ly = points[p].y - j * ddy;

        double dx_lx = ddx - lx;
        double dy_ly = ddy - ly;

        int idx = j * gx + i;

        double Fi = dx_lx * dy_ly * mesh_value[idx]
                  + lx    * dy_ly * mesh_value[idx + 1]
                  + dx_lx * ly    * mesh_value[idx + gx]
                  + lx    * ly    * mesh_value[idx + gx + 1];

        points[p].x += Fi * ddx;
        points[p].y += Fi * ddy;

        if (points[p].x < 0.0 || points[p].x > 1.0 ||
            points[p].y < 0.0 || points[p].y > 1.0) {
            points[p].is_void = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Denormalization: restore mesh to original range
// ---------------------------------------------------------------------------
void denormalization(double *mesh_value) {
    const int G_SIZE = GRID_X * GRID_Y;
    double range = max_val - min_val;

    if (range > 0.0) {
        double half_range = range / 2.0;
        #pragma omp parallel for schedule(static)
        for (int k = 0; k < G_SIZE; k++) {
            mesh_value[k] = (mesh_value[k] + 1.0) * half_range + min_val;
        }
    }
}

// Count void particles
long long int void_count(Points *points) {
    long long int voids = 0;
    #pragma omp parallel for reduction(+:voids)
    for (int i = 0; i < NUM_Points; i++) {
        voids += (int)points[i].is_void;
    }
    return voids;
}

// Write mesh to file
void save_mesh(double *mesh_value) {
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