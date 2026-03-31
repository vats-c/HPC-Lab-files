/* Experiment 2 – NO insertion/deletion (Assignment 04 style)
 * Baseline comparison for speedup plots.
 * 14M particles, Maxiter=10, threads = {2,4,8,16}.
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

    printf("=== Experiment 2: No Insert/Delete (Assign-04) – Parallel Scalability ===\n");
    printf("NX,NY,Threads,Serial_Mover,Parallel_Mover,Speedup\n");

    for (int g = 0; g < n_grids; g++) {
        NX = grids[g][0]; NY = grids[g][1];
        GRID_X = NX + 1;  GRID_Y = NY + 1;
        dx = 1.0 / NX;    dy = 1.0 / NY;

        /* Serial baseline */
        double *mesh = (double *)calloc((size_t)GRID_X * GRID_Y, sizeof(double));
        Points *pts  = (Points *)calloc((size_t)NUM_Points, sizeof(Points));
        if (!mesh || !pts) { printf("OOM\n"); return 1; }

        srand(42);
        initializepoints(pts);

        double serial_mover = 0.0;
        for (int it = 0; it < Maxiter; it++) {
            double t0 = omp_get_wtime();
            interpolation(mesh, pts);
            double t1 = omp_get_wtime();
            mover_serial(pts, dx, dy);
            double t2 = omp_get_wtime();
            serial_mover += (t2 - t1);
        }
        free(mesh); free(pts);

        /* Parallel runs */
        for (int t = 0; t < n_threads; t++) {
            omp_set_num_threads(thread_counts[t]);

            mesh = (double *)calloc((size_t)GRID_X * GRID_Y, sizeof(double));
            pts  = (Points *)calloc((size_t)NUM_Points, sizeof(Points));
            srand(42);
            initializepoints(pts);

            double par_mover = 0.0;
            for (int it = 0; it < Maxiter; it++) {
                double t0 = omp_get_wtime();
                interpolation(mesh, pts);
                double t1 = omp_get_wtime();
                mover_parallel(pts, dx, dy);
                double t2 = omp_get_wtime();
                par_mover += (t2 - t1);
            }
            double speedup = serial_mover / par_mover;

            printf("%d,%d,%d,%lf,%lf,%lf\n",
                   NX, NY, thread_counts[t],
                   serial_mover, par_mover, speedup);
            free(mesh); free(pts);
        }
    }
    return 0;
}
