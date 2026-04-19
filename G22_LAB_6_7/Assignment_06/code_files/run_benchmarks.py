#!/usr/bin/env python3
"""
HPC Assignment 06 - Benchmark Runner (Cross-platform)
Generates benchmark_results.csv for all 5 configurations.
Works on both Windows and Linux.
"""

import os
import subprocess
import sys
import platform

configs = [
    {"name": "a", "Nx": 250, "Ny": 100, "points": 900000, "Maxiter": 10},
    {"name": "b", "Nx": 250, "Ny": 100, "points": 5000000, "Maxiter": 10},
    {"name": "c", "Nx": 500, "Ny": 200, "points": 3600000, "Maxiter": 10},
    {"name": "d", "Nx": 500, "Ny": 200, "points": 20000000, "Maxiter": 10},
    {"name": "e", "Nx": 1000, "Ny": 400, "points": 14000000, "Maxiter": 10},
]

thread_counts = [2, 4, 8, 16]

# Detect platform
is_windows = platform.system() == "Windows"
ext = ".exe" if is_windows else ".out"

input_maker = f"./input_maker{ext}" if not is_windows else f".\\input_maker{ext}"
serial_bin = f"./serial_baseline{ext}" if not is_windows else f".\\serial_baseline{ext}"
parallel_bin = f"./main_parallel{ext}" if not is_windows else f".\\main_parallel{ext}"
# For Linux .out compiled as main.out
if not is_windows:
    parallel_bin = "./main.out"
    serial_bin = "./serial_baseline.out"
    input_maker = "./input_maker.out"


def extract_time(output_text):
    """Extract time in seconds from program output."""
    for line in output_text.split('\n'):
        if "seconds" in line and "Total interpolation time" in line:
            parts = line.split('=')
            if len(parts) > 1:
                return float(parts[1].replace('seconds', '').strip())
    return None


def run_with_threads(binary, input_file, num_threads):
    """Run a binary with specified number of OMP threads."""
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(num_threads)
    result = subprocess.run(
        [binary, input_file],
        env=env, capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr}")
        return None
    return extract_time(result.stdout)


print("=" * 60)
print("  HPC Assignment 06 - Benchmark Suite")
print("=" * 60)

results_file = "benchmark_results.csv"
with open(results_file, "w") as f_out:
    f_out.write("Config,Nx,Ny,Points,Threads,Time_seconds\n")

    for c in configs:
        print(f"\n{'='*60}")
        print(f"  Config {c['name']}: Nx={c['Nx']}, Ny={c['Ny']}, Points={c['points']}")
        print(f"{'='*60}")

        # Generate input
        print(f"  [Gen] Generating input.bin...")
        result = subprocess.run(
            [input_maker if not is_windows else input_maker,
             str(c['Nx']), str(c['Ny']), str(c['points']), str(c['Maxiter'])],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"  ERROR generating input: {result.stderr}")
            continue
        print(f"  [Gen] Done.")

        # Serial baseline
        print(f"  [Serial] Running serial baseline...")
        serial_time = run_with_threads(serial_bin, "input.bin", 1)
        if serial_time is not None:
            print(f"    Serial time: {serial_time:.6f} seconds")
            f_out.write(f"{c['name']},{c['Nx']},{c['Ny']},{c['points']},1,{serial_time:.6f}\n")
            f_out.flush()
        else:
            print(f"    ERROR: Could not get serial time")

        # Parallel runs
        for t in thread_counts:
            # Clean Mesh.out before each run
            if os.path.exists("Mesh.out"):
                os.remove("Mesh.out")

            print(f"  [Parallel] Running with {t} threads...")
            parallel_time = run_with_threads(parallel_bin, "input.bin", t)
            if parallel_time is not None:
                speedup = serial_time / parallel_time if serial_time and parallel_time > 0 else 0
                print(f"    {t} threads: {parallel_time:.6f} seconds (speedup: {speedup:.2f}x)")
                f_out.write(f"{c['name']},{c['Nx']},{c['Ny']},{c['points']},{t},{parallel_time:.6f}\n")
                f_out.flush()
            else:
                print(f"    ERROR: Could not get time for {t} threads")

        # Cleanup
        for fn in ["input.bin", "Mesh.out"]:
            if os.path.exists(fn):
                os.remove(fn)

print(f"\n{'='*60}")
print(f"  Benchmarks complete! Results saved to: {results_file}")
print(f"{'='*60}")
print(f"\nRun 'python plot_results.py' to generate plots.")
