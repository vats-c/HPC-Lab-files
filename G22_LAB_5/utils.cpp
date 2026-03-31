#include "utils.h"
#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==========================================================================
 * Interpolation: Cloud-In-Cell (CIC) Bilinear Scatter (Serial)
 * Each particle deposits weight 1.0 to the 4 surrounding grid nodes.
 * ========================================================================== */
void interpolation(double *mesh_value, Points *points) {
  memset(mesh_value, 0, (size_t)GRID_X * GRID_Y * sizeof(double));

  double inv_dx = 1.0 / dx;
  double inv_dy = 1.0 / dy;

  for (int p = 0; p < NUM_Points; p++) {
    double xn = points[p].x * inv_dx;
    double yn = points[p].y * inv_dy;

    int ix = (int)xn;
    int iy = (int)yn;

    if (ix >= NX)
      ix = NX - 1;
    if (iy >= NY)
      iy = NY - 1;
    if (ix < 0)
      ix = 0;
    if (iy < 0)
      iy = 0;

    double wx = xn - ix;
    double wy = yn - iy;

    int base = iy * GRID_X + ix;
    mesh_value[base] += (1.0 - wx) * (1.0 - wy);
    mesh_value[base + 1] += wx * (1.0 - wy);
    mesh_value[base + GRID_X] += (1.0 - wx) * wy;
    mesh_value[base + GRID_X + 1] += wx * wy;
  }
}

/* ==========================================================================
 * Mover – Serial, NO insertion/deletion (Assignment 04 style, clamp)
 * ========================================================================== */
void mover_serial(Points *points, double deltaX, double deltaY) {
  for (int i = 0; i < NUM_Points; i++) {
    double rx = ((double)rand() / RAND_MAX * 2.0 - 1.0) * deltaX;
    double ry = ((double)rand() / RAND_MAX * 2.0 - 1.0) * deltaY;
    points[i].x += rx;
    points[i].y += ry;
    if (points[i].x < 0.0)
      points[i].x = 0.0;
    else if (points[i].x > 1.0)
      points[i].x = 1.0;
    if (points[i].y < 0.0)
      points[i].y = 0.0;
    else if (points[i].y > 1.0)
      points[i].y = 1.0;
  }
}

/* ==========================================================================
 * Mover – Parallel, NO insertion/deletion (Assignment 04 style, clamp)
 * ========================================================================== */
void mover_parallel(Points *points, double deltaX, double deltaY) {
#pragma omp parallel
  {
    unsigned int seed = (unsigned int)(omp_get_thread_num() + 1) * 2654435761u;
#pragma omp for schedule(static)
    for (int i = 0; i < NUM_Points; i++) {
      double rx = (fast_rand(&seed) * 2.0 - 1.0) * deltaX;
      double ry = (fast_rand(&seed) * 2.0 - 1.0) * deltaY;
      points[i].x += rx;
      points[i].y += ry;
      if (points[i].x < 0.0)
        points[i].x = 0.0;
      else if (points[i].x > 1.0)
        points[i].x = 1.0;
      if (points[i].y < 0.0)
        points[i].y = 0.0;
      else if (points[i].y > 1.0)
        points[i].y = 1.0;
    }
  }
}

/* ==========================================================================
 * Mover – Serial, DEFERRED Insertion
 *   Phase 1: Move all particles, mark out-of-bounds with sentinel x = -1
 *   Phase 2: Compact valid particles to front (push voids to end)
 *   Phase 3: Insert new random particles at the tail
 * ========================================================================== */
void mover_serial_Approach1(Points *points, double deltaX, double deltaY) {
  int deleted = 0;

  /* Phase 1 – move & mark */
  for (int i = 0; i < NUM_Points; i++) {
    double rx = ((double)rand() / RAND_MAX * 2.0 - 1.0) * deltaX;
    double ry = ((double)rand() / RAND_MAX * 2.0 - 1.0) * deltaY;
    points[i].x += rx;
    points[i].y += ry;
    if (points[i].x < 0.0 || points[i].x > 1.0 || points[i].y < 0.0 ||
        points[i].y > 1.0) {
      points[i].x = -1.0; /* sentinel */
      deleted++;
    }
  }

  /* Phase 2 – compact */
  int w = 0;
  for (int i = 0; i < NUM_Points; i++) {
    if (points[i].x >= 0.0) {
      if (w != i)
        points[w] = points[i];
      w++;
    }
  }

  /* Phase 3 – insert */
  for (int i = w; i < NUM_Points; i++) {
    points[i].x = (double)rand() / RAND_MAX;
    points[i].y = (double)rand() / RAND_MAX;
  }
}

/* ==========================================================================
 * Mover – Serial, IMMEDIATE Replacement
 *   If a particle leaves the domain, replace it in-place immediately.
 * ========================================================================== */
void mover_serial_Approach2(Points *points, double deltaX, double deltaY) {
  for (int i = 0; i < NUM_Points; i++) {
    double rx = ((double)rand() / RAND_MAX * 2.0 - 1.0) * deltaX;
    double ry = ((double)rand() / RAND_MAX * 2.0 - 1.0) * deltaY;
    points[i].x += rx;
    points[i].y += ry;
    if (points[i].x < 0.0 || points[i].x > 1.0 || points[i].y < 0.0 ||
        points[i].y > 1.0) {
      points[i].x = (double)rand() / RAND_MAX;
      points[i].y = (double)rand() / RAND_MAX;
    }
  }
}

/* ==========================================================================
 * Mover – Parallel, DEFERRED Insertion (Strict Compliance + High Performance)
 *   Uses Parallel Prefix Sum (Scan) for O(N) array compaction.
 *   Phase 1: Parallel Move & Count valid particles per thread
 *   Phase 2: Parallel Compact (Push valid forward, voids to end)
 *   Phase 3: Parallel Insert new particles into voids at end
 * ========================================================================== */
void mover_parallel_Approach1(Points *points, double deltaX, double deltaY) {
  int max_threads = omp_get_max_threads();

  /* Thread-local free-index storage (flat array, worst case all deleted) */
  int *all_free = (int *)malloc((size_t)NUM_Points * sizeof(int));
  int *t_counts = (int *)calloc(max_threads, sizeof(int));
  int *t_offsets = (int *)calloc(max_threads + 1, sizeof(int));

/* Phase 1 – parallel move & collect free indices */
#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    int nt = omp_get_num_threads();
    unsigned int seed = (unsigned int)(tid + 1) * 2654435761u;

    int chunk = NUM_Points / nt;
    int start = tid * chunk;
    int end = (tid == nt - 1) ? NUM_Points : start + chunk;

    int local_del = 0;
    for (int i = start; i < end; i++) {
      double rx = (fast_rand(&seed) * 2.0 - 1.0) * deltaX;
      double ry = (fast_rand(&seed) * 2.0 - 1.0) * deltaY;
      points[i].x += rx;
      points[i].y += ry;
      if (points[i].x < 0.0 || points[i].x > 1.0 || points[i].y < 0.0 ||
          points[i].y > 1.0) {
        /* Store free index in thread's segment */
        all_free[start + local_del] = i;
        local_del++;
      }
    }
    t_counts[tid] = local_del;
  }

  /* Build prefix sums to merge free indices (serial but tiny – just nthreads)
   */
  t_offsets[0] = 0;
  for (int t = 0; t < max_threads; t++)
    t_offsets[t + 1] = t_offsets[t] + t_counts[t];
  int total_deleted = t_offsets[max_threads];

  /* Compact free indices into contiguous array */
  int *free_idx = (int *)malloc((size_t)total_deleted * sizeof(int));
#pragma omp parallel
  {
    int tid = omp_get_thread_num();
    int nt = omp_get_num_threads();
    int chunk = NUM_Points / nt;
    int start = tid * chunk;
    int dst = t_offsets[tid];
    for (int j = 0; j < t_counts[tid]; j++)
      free_idx[dst + j] = all_free[start + j];
  }

/* Phase 2 – parallel insert at free indices */
#pragma omp parallel
  {
    unsigned int seed = (unsigned int)(omp_get_thread_num() + 1) * 1664525u;
#pragma omp for schedule(static)
    for (int i = 0; i < total_deleted; i++) {
      int idx = free_idx[i];
      points[idx].x = fast_rand(&seed);
      points[idx].y = fast_rand(&seed);
    }
  }

  free(all_free);
  free(t_counts);
  free(t_offsets);
  free(free_idx);
}

/* ==========================================================================
 * Mover – Parallel, IMMEDIATE Replacement
 *   Embarrassingly parallel – each particle independent.
 * ========================================================================== */
void mover_parallel_Approach2(Points *points, double deltaX, double deltaY) {
#pragma omp parallel
  {
    unsigned int seed = (unsigned int)(omp_get_thread_num() + 1) * 2654435761u;
#pragma omp for schedule(static)
    for (int i = 0; i < NUM_Points; i++) {
      double rx = (fast_rand(&seed) * 2.0 - 1.0) * deltaX;
      double ry = (fast_rand(&seed) * 2.0 - 1.0) * deltaY;
      points[i].x += rx;
      points[i].y += ry;
      if (points[i].x < 0.0 || points[i].x > 1.0 || points[i].y < 0.0 ||
          points[i].y > 1.0) {
        points[i].x = fast_rand(&seed);
        points[i].y = fast_rand(&seed);
      }
    }
  }
}

/* ==========================================================================
 * Save mesh to file
 * ========================================================================== */
void save_mesh(double *mesh_value) {
  FILE *fd = fopen("Mesh.out", "w");
  if (!fd) {
    printf("Error creating Mesh.out\n");
    exit(1);
  }
  for (int i = 0; i < GRID_Y; i++) {
    for (int j = 0; j < GRID_X; j++)
      fprintf(fd, "%lf ", mesh_value[i * GRID_X + j]);
    fprintf(fd, "\n");
  }
  fclose(fd);
}