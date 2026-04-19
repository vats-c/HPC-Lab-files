#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define INPUT_FILENAME "input.bin"

void generate_input_file(int NX, int NY, int NUM_Points, int Maxiter) {
    FILE *fp = fopen(INPUT_FILENAME, "wb");
    if (fp == NULL) {
        perror("Error: Unable to create file input.bin\n");
        exit(EXIT_FAILURE);
    }

    fwrite(&NX, sizeof(int), 1, fp);
    fwrite(&NY, sizeof(int), 1, fp);
    fwrite(&NUM_Points, sizeof(int), 1, fp);
    fwrite(&Maxiter, sizeof(int), 1, fp);

    srand((unsigned int)time(NULL));

    for (int i = 0; i < NUM_Points; i++) {
        double x = (double)rand() / RAND_MAX;
        double y = (double)rand() / RAND_MAX;
        fwrite(&x, sizeof(double), 1, fp);
        fwrite(&y, sizeof(double), 1, fp);
    }

    fclose(fp);
    printf("Input file '%s' generated: NX=%d, NY=%d, Points=%d, Maxiter=%d\n",
           INPUT_FILENAME, NX, NY, NUM_Points, Maxiter);
}

int main(int argc, char **argv) {
    int NX, NY, NUM_Points, Maxiter;

    if (argc == 5) {
        // Command-line mode (for automated benchmarking)
        NX = atoi(argv[1]);
        NY = atoi(argv[2]);
        NUM_Points = atoi(argv[3]);
        Maxiter = atoi(argv[4]);
    } else if (argc == 1) {
        // Interactive mode
        printf("Enter grid dimensions (NX NY): ");
        scanf("%d %d", &NX, &NY);
        printf("Enter number of particles: ");
        scanf("%d", &NUM_Points);
        printf("Enter number of iterations: ");
        scanf("%d", &Maxiter);
    } else {
        printf("Usage: %s [NX NY NUM_Points Maxiter]\n", argv[0]);
        return 1;
    }

    generate_input_file(NX, NY, NUM_Points, Maxiter);
    return 0;
}