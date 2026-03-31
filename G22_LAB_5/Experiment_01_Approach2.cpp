/* Experiment 1 – Approach2 (Immediate) Replacement Approach
 * Serial mover scaling with particle count across 3 grid configs.
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
    int grids[][2] = {{250,100},{500,200},{1000,400}};
    int particle_counts[] = {100, 10000, 1000000, 100000000};
    int n_pc = 4;
    int n_grids = 3;

    printf("=== Experiment 1: Approach2 (Immediate Replacement) (Serial) ===\n");
    printf("NX,NY,Particles,Interp_Time,Mover_Time,Total_Time\n");

    for (int g = 0; g < n_grids; g++) {
        NX = grids[g][0]; NY = grids[g][1];
        GRID_X = NX + 1;  GRID_Y = NY + 1;
        dx = 1.0 / NX;    dy = 1.0 / NY;

        for (int p = 0; p < n_pc; p++) {
            NUM_Points = particle_counts[p];

            double *mesh = (double *)calloc((size_t)GRID_X * GRID_Y, sizeof(double));
            Points *pts  = (Points *)calloc((size_t)NUM_Points, sizeof(Points));
            if (!mesh || !pts) {
                printf("%d,%d,%d,OOM,OOM,OOM\n", NX, NY, NUM_Points);
                free(mesh); free(pts); continue;
            }

            srand(42);
            initializepoints(pts);

            double t_interp = 0.0, t_mover = 0.0;
            for (int it = 0; it < Maxiter; it++) {
                double t0 = omp_get_wtime();
                interpolation(mesh, pts);
                double t1 = omp_get_wtime();
                mover_serial_Approach2(pts, dx, dy);
                double t2 = omp_get_wtime();
                t_interp += (t1 - t0);
                t_mover  += (t2 - t1);
            }
            printf("%d,%d,%d,%lf,%lf,%lf\n", NX, NY, NUM_Points,
                   t_interp, t_mover, t_interp + t_mover);
            free(mesh); free(pts);
        }
    }
    return 0;
}
