#ifndef UTILS_H
#define UTILS_H
#include <time.h>
#include "init.h"

// PIC operations
void interpolation(double *local_mesh, double *global_mesh, Points *points, int local_num, double *timings);
void save_mesh(double *mesh_value);

#endif
