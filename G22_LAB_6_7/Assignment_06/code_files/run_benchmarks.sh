#!/bin/bash
# =============================================================================
# HPC Assignment 06 - Full Benchmark Script
# Runs serial baseline + parallel with 2,4,8,16 threads for all 5 configurations
# =============================================================================

set -e

echo "=========================================="
echo "  HPC Assignment 06 - Benchmark Suite"
echo "=========================================="

# Build everything
echo "[1/4] Building executables..."
make clean
make all

# Output CSV file
RESULTS_FILE="benchmark_results.csv"
echo "Config,Nx,Ny,Points,Threads,Time_seconds" > $RESULTS_FILE

# 5 Configurations from the assignment
declare -a NAMES=("a" "b" "c" "d" "e")
declare -a NX_VALS=(250 250 500 500 1000)
declare -a NY_VALS=(100 100 200 200 400)
declare -a PT_VALS=(900000 5000000 3600000 20000000 14000000)
MAXITER=10

THREAD_COUNTS="2 4 8 16"

for idx in 0 1 2 3 4; do
    NAME=${NAMES[$idx]}
    NX=${NX_VALS[$idx]}
    NY=${NY_VALS[$idx]}
    PTS=${PT_VALS[$idx]}

    echo ""
    echo "=========================================="
    echo "  Config $NAME: Nx=$NX, Ny=$NY, Points=$PTS"
    echo "=========================================="

    # Generate input file
    echo "[Gen] Generating input.bin..."
    ./input_maker.out $NX $NY $PTS $MAXITER

    # --- Serial baseline ---
    echo "[Serial] Running serial baseline..."
    SERIAL_OUTPUT=$(./serial_baseline.out input.bin 2>&1)
    SERIAL_TIME=$(echo "$SERIAL_OUTPUT" | grep "seconds" | awk -F'=' '{print $2}' | awk '{print $1}')
    echo "  Serial time: $SERIAL_TIME seconds"
    echo "$NAME,$NX,$NY,$PTS,1,$SERIAL_TIME" >> $RESULTS_FILE

    # --- Parallel runs ---
    for T in $THREAD_COUNTS; do
        echo "[Parallel] Running with $T threads..."
        export OMP_NUM_THREADS=$T
        PARALLEL_OUTPUT=$(./main.out input.bin 2>&1)
        PARALLEL_TIME=$(echo "$PARALLEL_OUTPUT" | grep "seconds" | awk -F'=' '{print $2}' | awk '{print $1}')
        echo "  $T threads: $PARALLEL_TIME seconds"
        echo "$NAME,$NX,$NY,$PTS,$T,$PARALLEL_TIME" >> $RESULTS_FILE
    done

    # Cleanup to save disk space
    rm -f input.bin Mesh.out
done

echo ""
echo "=========================================="
echo "  Benchmarks complete! Results in: $RESULTS_FILE"
echo "=========================================="
cat $RESULTS_FILE
