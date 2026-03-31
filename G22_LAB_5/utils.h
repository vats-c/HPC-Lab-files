#ifndef UTILS_H
#define UTILS_H
#include <time.h>
#include "init.h"

// Thread-safe fast RNG (LCG)
static inline double fast_rand(unsigned int *state) {
    *state = (*state) * 1103515245u + 12345u;
    return (double)((*state >> 16) & 0x7FFF) / 32767.0;
}

// Interpolation
void interpolation(double *mesh_value, Points *points);

// Mover: Assignment-04 style (clamp to boundary, no insert/delete)
void mover_serial(Points *points, double deltaX, double deltaY);
void mover_parallel(Points *points, double deltaX, double deltaY);

// Mover: Approach1 (Deferred)
void mover_serial_Approach1(Points *points, double deltaX, double deltaY);
void mover_parallel_Approach1(Points *points, double deltaX, double deltaY);

// Mover: Approach2 (Immediate)
void mover_serial_Approach2(Points *points, double deltaX, double deltaY);
void mover_parallel_Approach2(Points *points, double deltaX, double deltaY);

// I/O
void save_mesh(double *mesh_value);

#endif
