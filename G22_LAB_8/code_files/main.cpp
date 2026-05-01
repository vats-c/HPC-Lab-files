#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "init.h"
#include "utils.h"

// Global variables
int GRID_X, GRID_Y, NX, NY;
int NUM_Points, Maxiter;
double dx, dy;

int main(int argc, char **argv) {

    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 2) {
        if (rank == 0) printf("Usage: %s <input_file>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    // Open binary file for reading
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        if (rank == 0) printf("Error opening input file\n");
        MPI_Finalize();
        return 1;
    }

    // Read grid dimensions
    fread(&NX, sizeof(int), 1, file);
    fread(&NY, sizeof(int), 1, file);

    // Read number of Points and max iterations
    fread(&NUM_Points, sizeof(int), 1, file);
    fread(&Maxiter, sizeof(int), 1, file);

    GRID_X = NX + 1;
    GRID_Y = NY + 1;
    dx = 1.0 / NX;
    dy = 1.0 / NY;

    int local_num = NUM_Points / size;
    int remainder = NUM_Points % size;
    if (rank < remainder) {
        local_num++;
    }

    long points_before = 0;
    if (rank < remainder) {
        points_before = (long)rank * local_num;
    } else {
        points_before = (long)remainder * (local_num + 1) + (long)(rank - remainder) * local_num;
    }

    long offset = 4 * sizeof(int) + points_before * 2 * sizeof(double);

    // Allocate memory for points
    Points *points = (Points *) calloc(local_num, sizeof(Points));

    // Read scattered points once (they evolve)
    read_points(file, points, local_num, offset);
    fclose(file);

    double *local_mesh = (double *) calloc(GRID_X * GRID_Y, sizeof(double));
    double *global_mesh = (double *) calloc(GRID_X * GRID_Y, sizeof(double));

    double total_time = 0.0;
    double timings[4] = {0.0, 0.0, 0.0, 0.0}; // Interp, Comm, Norm, Mover

    for (int iter = 0; iter < Maxiter; iter++) {

        double start = MPI_Wtime();

        // Perform hybrid interpolation + mover pipeline
        interpolation(local_mesh, global_mesh, points, local_num, timings);

        double end = MPI_Wtime();
        total_time += (end - start);
    }

    if (rank == 0) {
        save_mesh(global_mesh);
        printf("Total Time: %lf\n", total_time);
        printf("Interpolation: %lf\n", timings[0]);
        printf("MPI Comm: %lf\n", timings[1]);
        printf("Normalization: %lf\n", timings[2]);
        printf("Mover: %lf\n", timings[3]);
    }

    // Free memory
    free(local_mesh);
    free(global_mesh);
    free(points);

    MPI_Finalize();
    return 0;
}