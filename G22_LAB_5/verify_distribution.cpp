/* verify_distribution.cpp
 * Saves particle positions to CSV for distribution verification plots.
 * Runs a few mover iterations, then dumps positions.
 */
#include "init.h"
#include "utils.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int GRID_X, GRID_Y, NX, NY;
int NUM_Points, Maxiter;
double dx, dy;

void save_positions(Points *pts, int n, const char *filename) {
  FILE *f = fopen(filename, "w");
  if (!f) {
    printf("Error opening %s\n", filename);
    return;
  }
  fprintf(f, "x,y\n");
  for (int i = 0; i < n; i++)
    fprintf(f, "%.8f,%.8f\n", pts[i].x, pts[i].y);
  fclose(f);
  printf("Saved %d positions to %s\n", n, filename);
}

int main() {
  NX = 250;
  NY = 100;
  GRID_X = NX + 1;
  GRID_Y = NY + 1;
  dx = 1.0 / NX;
  dy = 1.0 / NY;
  NUM_Points = 100000;
  Maxiter = 5;

  double *mesh = (double *)calloc((size_t)GRID_X * GRID_Y, sizeof(double));
  Points *pts = (Points *)calloc((size_t)NUM_Points, sizeof(Points));

  /* Initial distribution */
  srand(42);
  initializepoints(pts);
  save_positions(pts, NUM_Points, "positions_initial.csv");

  /* After Approach1 (deferred) mover iterations */
  srand(42);
  initializepoints(pts);
  for (int it = 0; it < Maxiter; it++) {
    interpolation(mesh, pts);
    mover_serial_Approach1(pts, dx, dy);
  }
  save_positions(pts, NUM_Points, "positions_after_Approach1.csv");

  /* After Approach2 (immediate) mover iterations */
  srand(42);
  initializepoints(pts);
  for (int it = 0; it < Maxiter; it++) {
    interpolation(mesh, pts);
    mover_serial_Approach2(pts, dx, dy);
  }
  save_positions(pts, NUM_Points, "positions_after_Approach2.csv");

  /* Save mesh for verification */
  save_mesh(mesh);
  printf("Mesh saved to Mesh.out\n");

  free(mesh);
  free(pts);
  return 0;
}
