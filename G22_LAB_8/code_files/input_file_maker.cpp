#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <NX> <NY> <NUM_Points> <Maxiter>\n", argv[0]);
        return 1;
    }

    int NX = atoi(argv[1]);
    int NY = atoi(argv[2]);
    int NUM_Points = atoi(argv[3]);
    int Maxiter = atoi(argv[4]);

    FILE *fp = fopen("input.bin", "wb");
    if (!fp) {
        perror("Error opening input.bin");
        return 1;
    }

    fwrite(&NX, sizeof(int), 1, fp);
    fwrite(&NY, sizeof(int), 1, fp);
    fwrite(&NUM_Points, sizeof(int), 1, fp);
    fwrite(&Maxiter, sizeof(int), 1, fp);

    srand(42); // Fixed seed for reproducibility

    // Only write ONE set of initial particles (since they evolve in Assignment 8)
    for (int i = 0; i < NUM_Points; i++) {
        double x = (double)rand() / RAND_MAX;
        double y = (double)rand() / RAND_MAX;
        fwrite(&x, sizeof(double), 1, fp);
        fwrite(&y, sizeof(double), 1, fp);
    }

    fclose(fp);
    printf("Generated input.bin: %d x %d, %d points, %d iterations\n", NX, NY, NUM_Points, Maxiter);
    return 0;
}