#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "init.h"

// Random particle initialization (optional)
void initializepoints(Points *points, int local_num) {
    for (int i = 0; i < local_num; i++) {
        points[i].x = (double) rand() / RAND_MAX;
        points[i].y = (double) rand() / RAND_MAX;
        points[i].is_void = false;
    }
}

// Read particle positions from binary file
void read_points(FILE *file, Points *points, int local_num, long offset) {
    fseek(file, offset, SEEK_SET);
    for (int i = 0; i < local_num; i++) {
        fread(&points[i].x, sizeof(double), 1, file);
        fread(&points[i].y, sizeof(double), 1, file);
        points[i].is_void = false;
    }
}