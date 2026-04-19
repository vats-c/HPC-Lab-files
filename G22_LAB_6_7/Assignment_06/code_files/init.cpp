#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "init.h"

// Random particle initialization (optional)
void initializepoints(Points *points) {
    for (int i = 0; i < NUM_Points; i++) {
        points[i].x = (double) rand() / RAND_MAX;
        points[i].y = (double) rand() / RAND_MAX;
    }
}

// Read particle positions from binary file (bulk read for performance)
void read_points(FILE *file, Points *points) {
    fread(points, sizeof(Points), NUM_Points, file);
}