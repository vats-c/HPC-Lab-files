#!/usr/bin/env python3
"""
HPC Assignment 07 - Benchmark Runner (Cross-platform)
Generates benchmark_results.csv for all 5 configurations.
Works on both Windows and Linux.
"""

import os
import subprocess
import sys
import platform
import re

configs = [
    {"name": "a", "Nx": 250, "Ny": 100, "points": 900000,   "Maxiter": 10},
    {"name": "b", "Nx": 250, "Ny": 100, "points": 5000000,  "Maxiter": 10},
    {"name": "c", "Nx": 500, "Ny": 200, "points": 3600000,  "Maxiter": 10},
    {"name": "d", "Nx": 500, "Ny": 200, "points": 20000000, "Maxiter": 10},
    {"name": "e", "Nx": 1000,"Ny": 400, "points": 14000000, "Maxiter": 10},
]

thread_counts = [2, 4, 8, 16]

# Detect platform
is_windows = platform.system() == "Windows"
ext = ".exe" if is_windows else ".out"
prefix = ".\\" if is_windows else "./"

input_maker = f"{prefix}input_maker{ext}"
serial_bin  = f"{prefix}serial_baseline{ext}"
parallel_bin = f"{prefix}main{ext}"


def extract_times(output_text):
    """Extract per-phase timings from program output."""
    result = {}
    patterns = {
        "interp":  r"Total Interpolation Time\s*=\s*([\d.]+)\s*seconds",
        "norm":    r"Total Normalization Time\s*=\s*([\d.]+)\s*seconds",
        "mover":   r"Total Mover Time\s*=\s*([\d.]+)\s*seconds",
        "denorm":  r"Total Denormalization Time\s*=\s*([\d.]+)\s*seconds",
        "total":   r"Total Algorithm Time\s*=\s*([\d.]+)\s*seconds",
        "voids":   r"Total Number of Voids\s*=\s*(\d+)",
    }
    for key, pat in patterns.items():
        m = re.search(pat, output_text)
        if m:
            result[key] = float(m.group(1)) if key != "voids" else int(m.group(1))
    return result


def run_program(binary, input_file, num_threads=None):
    """Run a binary with optional OMP thread count."""
    env = os.environ.copy()
    if num_threads is not None:
        env["OMP_NUM_THREADS"] = str(num_threads)
    result = subprocess.run(
        [binary, input_file],
        env=env, capture_output=True, text=True, timeout=600
    )
    if result.returncode != 0:
        print(f"  ERROR: {result.stderr[:200]}")
        return None
    return extract_times(result.stdout)


print("=" * 65)
print("  HPC Assignment 07 - Benchmark Suite")
print("=" * 65)

results_file = "benchmark_results.csv"
with open(results_file, "w") as f_out:
    f_out.write("Config,Nx,Ny,Points,Threads,Interp_Time,Norm_Time,Mover_Time,Denorm_Time,Total_Time,Voids\n")

    for c in configs:
        print(f"\n{'='*65}")
        print(f"  Config {c['name']}: Nx={c['Nx']}, Ny={c['Ny']}, Points={c['points']:,}")
        print(f"{'='*65}")

        # Generate input
        print(f"  [Gen] Generating input.bin...")
        result = subprocess.run(
            [input_maker, str(c['Nx']), str(c['Ny']), str(c['points']), str(c['Maxiter'])],
            capture_output=True, text=True
        )
        if result.returncode != 0:
            print(f"  ERROR generating input: {result.stderr[:200]}")
            continue
        print(f"  [Gen] Done.")

        # Serial baseline
        print(f"  [Serial] Running serial baseline...")
        serial_data = run_program(serial_bin, "input.bin")
        if serial_data and "total" in serial_data:
            serial_time = serial_data["total"]
            print(f"    Serial total: {serial_time:.6f}s  (interp:{serial_data.get('interp',0):.4f}s, mover:{serial_data.get('mover',0):.4f}s)")
            f_out.write(f"{c['name']},{c['Nx']},{c['Ny']},{c['points']},1,"
                        f"{serial_data.get('interp',0):.6f},{serial_data.get('norm',0):.6f},"
                        f"{serial_data.get('mover',0):.6f},{serial_data.get('denorm',0):.6f},"
                        f"{serial_time:.6f},{serial_data.get('voids',0)}\n")
            f_out.flush()
        else:
            print(f"    ERROR: Could not get serial time")
            serial_time = None

        # Parallel runs
        for t in thread_counts:
            if os.path.exists("Mesh.out"):
                os.remove("Mesh.out")

            print(f"  [Parallel] Running with {t} threads...")
            par_data = run_program(parallel_bin, "input.bin", t)
            if par_data and "total" in par_data:
                par_time = par_data["total"]
                speedup = serial_time / par_time if serial_time and par_time > 0 else 0
                print(f"    {t} threads: {par_time:.6f}s (speedup: {speedup:.2f}x) "
                      f"[interp:{par_data.get('interp',0):.4f}s mover:{par_data.get('mover',0):.4f}s]")
                f_out.write(f"{c['name']},{c['Nx']},{c['Ny']},{c['points']},{t},"
                            f"{par_data.get('interp',0):.6f},{par_data.get('norm',0):.6f},"
                            f"{par_data.get('mover',0):.6f},{par_data.get('denorm',0):.6f},"
                            f"{par_time:.6f},{par_data.get('voids',0)}\n")
                f_out.flush()
            else:
                print(f"    ERROR: Could not get time for {t} threads")

        # Cleanup
        for fn in ["input.bin", "Mesh.out"]:
            if os.path.exists(fn):
                os.remove(fn)

print(f"\n{'='*65}")
print(f"  Benchmarks complete! Results saved to: {results_file}")
print(f"{'='*65}")
print(f"\nRun 'python plot_results.py' to generate plots.")
