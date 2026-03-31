/* Experiment 2 – Approach1 (Deferred) Insertion: Parallel Scalability
 * 14M particles, Maxiter=10, threads = {1,2,4,8,16}.
 * Reports serial time, parallel mover time, and speedup for each grid.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "init.h"
#include "utils.h"

int GRID_X, GRID_Y, NX, NY;
int NUM_Points, Maxiter;
double dx, dy;

int main() {
    Maxiter = 10;
    NUM_Points = 14000000;
    int grids[][2] = {{250,100},{500,200},{1000,400}};
    int thread_counts[] = {2, 4, 8, 16};
    int n_threads = 4;
    int n_grids = 3;

    printf("=== Experiment 2: Approach1 (Deferred Insertion) – Parallel Scalability ===\n");
    printf("NX,NY,Threads,Serial_Mover,Parallel_Mover,Speedup,Serial_Total,Parallel_Total\n");

    for (int g = 0; g < n_grids; g++) {
        NX = grids[g][0]; NY = grids[g][1];
        GRID_X = NX + 1;  GRID_Y = NY + 1;
        dx = 1.0 / NX;    dy = 1.0 / NY;

        /* --- Serial baseline --- */
        double *mesh = (double *)calloc((size_t)GRID_X * GRID_Y, sizeof(double));
        Points *pts  = (Points *)calloc((size_t)NUM_Points, sizeof(Points));
        if (!mesh || !pts) { printf("OOM\n"); return 1; }

        srand(42);
        initializepoints(pts);

        double serial_interp = 0.0, serial_mover = 0.0;
        for (int it = 0; it < Maxiter; it++) {
            double t0 = omp_get_wtime();
            interpolation(mesh, pts);
            double t1 = omp_get_wtime();
            mover_serial_Approach1(pts, dx, dy);
            double t2 = omp_get_wtime();
            serial_interp += (t1 - t0);
            serial_mover  += (t2 - t1);
        }
        double serial_total = serial_interp + serial_mover;
        free(mesh); free(pts);

        /* --- Parallel runs --- */
        for (int t = 0; t < n_threads; t++) {
            omp_set_num_threads(thread_counts[t]);

            mesh = (double *)calloc((size_t)GRID_X * GRID_Y, sizeof(double));
            pts  = (Points *)calloc((size_t)NUM_Points, sizeof(Points));
            srand(42);
            initializepoints(pts);

            double par_interp = 0.0, par_mover = 0.0;
            for (int it = 0; it < Maxiter; it++) {
                double t0 = omp_get_wtime();
                interpolation(mesh, pts);
                double t1 = omp_get_wtime();
                mover_parallel_Approach1(pts, dx, dy);
                double t2 = omp_get_wtime();
                par_interp += (t1 - t0);
                par_mover  += (t2 - t1);
            }
            double par_total = par_interp + par_mover;
            double speedup = serial_mover / par_mover;

            printf("%d,%d,%d,%lf,%lf,%lf,%lf,%lf\n",
                   NX, NY, thread_counts[t],
                   serial_mover, par_mover, speedup,
                   serial_total, par_total);
            free(mesh); free(pts);
        }
    }
    return 0;
}
