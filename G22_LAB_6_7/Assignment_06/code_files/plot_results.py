#!/usr/bin/env python3
"""
HPC Assignment 06 - Plotting & Analysis Script
Generates:
  1. Speedup vs Cores (for each config)
  2. Execution Time vs Cores (for each config)
  3. Parallel Efficiency table
  4. Summary statistics
"""

import csv
import os
import sys

# Try to import matplotlib; if not available, generate tables only
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("WARNING: matplotlib not found. Will generate tables but no plots.")

# --- Load data ---
csv_file = "benchmark_results.csv"
if not os.path.exists(csv_file):
    print(f"Error: {csv_file} not found. Run run_benchmarks.sh first.")
    sys.exit(1)

data = {}  # data[config] = {threads: time}
config_info = {}  # config_info[config] = (Nx, Ny, Points)

with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        cfg = row['Config']
        threads = int(row['Threads'])
        time_val = float(row['Time_seconds'])
        nx = int(row['Nx'])
        ny = int(row['Ny'])
        pts = int(row['Points'])

        if cfg not in data:
            data[cfg] = {}
            config_info[cfg] = (nx, ny, pts)
        data[cfg][threads] = time_val

# --- Compute speedup and efficiency ---
print("\n" + "="*80)
print("  HPC Assignment 06 - Performance Analysis")
print("="*80)

configs_sorted = sorted(data.keys())
all_threads = sorted(set(t for cfg_data in data.values() for t in cfg_data.keys()))

# Speedup and Efficiency tables
print(f"\n{'Config':<10} {'Nx':<6} {'Ny':<6} {'Points':<12}", end="")
for t in all_threads:
    print(f"  T={t:<4}", end="")
print()
print("-" * (40 + 10 * len(all_threads)))

print("\n--- Execution Times (seconds) ---")
for cfg in configs_sorted:
    nx, ny, pts = config_info[cfg]
    print(f"  {cfg:<8} {nx:<6} {ny:<6} {pts:<12}", end="")
    for t in all_threads:
        if t in data[cfg]:
            print(f"  {data[cfg][t]:<8.4f}", end="")
        else:
            print(f"  {'N/A':<8}", end="")
    print()

print("\n--- Speedup (Serial_Time / Parallel_Time) ---")
speedup_data = {}
for cfg in configs_sorted:
    serial_time = data[cfg].get(1, None)
    speedup_data[cfg] = {}
    nx, ny, pts = config_info[cfg]
    print(f"  {cfg:<8} {nx:<6} {ny:<6} {pts:<12}", end="")
    for t in all_threads:
        if serial_time and t in data[cfg]:
            s = serial_time / data[cfg][t]
            speedup_data[cfg][t] = s
            print(f"  {s:<8.2f}", end="")
        else:
            print(f"  {'N/A':<8}", end="")
    print()

print("\n--- Parallel Efficiency (Speedup / Num_Threads * 100%) ---")
for cfg in configs_sorted:
    nx, ny, pts = config_info[cfg]
    print(f"  {cfg:<8} {nx:<6} {ny:<6} {pts:<12}", end="")
    for t in all_threads:
        if t in speedup_data[cfg]:
            eff = (speedup_data[cfg][t] / t) * 100
            print(f"  {eff:<7.1f}%", end="")
        else:
            print(f"  {'N/A':<8}", end="")
    print()

# Maximum speedup
print("\n--- Maximum Speedup Achieved ---")
for cfg in configs_sorted:
    nx, ny, pts = config_info[cfg]
    max_s = max(speedup_data[cfg].values()) if speedup_data[cfg] else 0
    max_t = max(speedup_data[cfg], key=speedup_data[cfg].get) if speedup_data[cfg] else 0
    print(f"  Config {cfg} (Nx={nx}, Ny={ny}, Pts={pts}): {max_s:.2f}x at {max_t} threads")

# --- Plots ---
if HAS_MATPLOTLIB:
    colors = ['#2196F3', '#FF5722', '#4CAF50', '#9C27B0', '#FF9800']
    markers = ['o', 's', '^', 'D', 'v']

    config_labels = {}
    for cfg in configs_sorted:
        nx, ny, pts = config_info[cfg]
        pts_str = f"{pts/1e6:.1f}M" if pts >= 1e6 else f"{pts/1e3:.0f}K"
        config_labels[cfg] = f"Config {cfg} ({nx}x{ny}, {pts_str} pts)"

    # ---- Plot 1: Speedup vs Cores ----
    fig, ax = plt.subplots(figsize=(10, 7))
    for i, cfg in enumerate(configs_sorted):
        threads_list = sorted(speedup_data[cfg].keys())
        speedups = [speedup_data[cfg][t] for t in threads_list]
        ax.plot(threads_list, speedups, marker=markers[i], color=colors[i],
                linewidth=2, markersize=8, label=config_labels[cfg])

    # Ideal speedup line
    ax.plot(all_threads, all_threads, 'k--', linewidth=1, alpha=0.5, label='Ideal Speedup')

    ax.set_xlabel('Number of Threads', fontsize=13)
    ax.set_ylabel('Speedup', fontsize=13)
    ax.set_title('Speedup vs Number of Threads', fontsize=15, fontweight='bold')
    ax.set_xticks(all_threads)
    ax.legend(fontsize=10, loc='upper left')
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, max(all_threads) + 1)
    plt.tight_layout()
    plt.savefig('speedup_vs_cores.png', dpi=150, bbox_inches='tight')
    print("\nSaved: speedup_vs_cores.png")

    # ---- Plot 2: Execution Time vs Cores ----
    fig, ax = plt.subplots(figsize=(10, 7))
    for i, cfg in enumerate(configs_sorted):
        threads_list = sorted(data[cfg].keys())
        times = [data[cfg][t] for t in threads_list]
        ax.plot(threads_list, times, marker=markers[i], color=colors[i],
                linewidth=2, markersize=8, label=config_labels[cfg])

    ax.set_xlabel('Number of Threads', fontsize=13)
    ax.set_ylabel('Execution Time (seconds)', fontsize=13)
    ax.set_title('Execution Time vs Number of Threads', fontsize=15, fontweight='bold')
    ax.set_xticks(all_threads)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, max(all_threads) + 1)
    plt.tight_layout()
    plt.savefig('execution_time_vs_cores.png', dpi=150, bbox_inches='tight')
    print("Saved: execution_time_vs_cores.png")

    # ---- Plot 3: Parallel Efficiency vs Cores ----
    fig, ax = plt.subplots(figsize=(10, 7))
    for i, cfg in enumerate(configs_sorted):
        threads_list = sorted(speedup_data[cfg].keys())
        efficiency = [(speedup_data[cfg][t] / t) * 100 for t in threads_list]
        ax.plot(threads_list, efficiency, marker=markers[i], color=colors[i],
                linewidth=2, markersize=8, label=config_labels[cfg])

    ax.axhline(y=100, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Ideal (100%)')
    ax.set_xlabel('Number of Threads', fontsize=13)
    ax.set_ylabel('Parallel Efficiency (%)', fontsize=13)
    ax.set_title('Parallel Efficiency vs Number of Threads', fontsize=15, fontweight='bold')
    ax.set_xticks(all_threads)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0.5, max(all_threads) + 1)
    ax.set_ylim(0, 120)
    plt.tight_layout()
    plt.savefig('parallel_efficiency_vs_cores.png', dpi=150, bbox_inches='tight')
    print("Saved: parallel_efficiency_vs_cores.png")

    # ---- Plot 4: Individual config plots (speedup + time side by side) ----
    for i, cfg in enumerate(configs_sorted):
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
        nx, ny, pts = config_info[cfg]

        threads_list = sorted(data[cfg].keys())
        times = [data[cfg][t] for t in threads_list]
        speedups = [speedup_data[cfg].get(t, 1.0) for t in threads_list]

        # Time plot
        ax1.plot(threads_list, times, marker='o', color=colors[i], linewidth=2, markersize=8)
        ax1.set_xlabel('Number of Threads', fontsize=12)
        ax1.set_ylabel('Execution Time (seconds)', fontsize=12)
        ax1.set_title(f'Execution Time - {config_labels[cfg]}', fontsize=12, fontweight='bold')
        ax1.set_xticks(all_threads)
        ax1.grid(True, alpha=0.3)

        # Speedup plot
        ax2.plot(threads_list, speedups, marker='s', color=colors[i], linewidth=2, markersize=8, label='Actual')
        ax2.plot(threads_list, threads_list, 'k--', linewidth=1, alpha=0.5, label='Ideal')
        ax2.set_xlabel('Number of Threads', fontsize=12)
        ax2.set_ylabel('Speedup', fontsize=12)
        ax2.set_title(f'Speedup - {config_labels[cfg]}', fontsize=12, fontweight='bold')
        ax2.set_xticks(all_threads)
        ax2.legend(fontsize=10)
        ax2.grid(True, alpha=0.3)

        plt.tight_layout()
        plt.savefig(f'config_{cfg}_analysis.png', dpi=150, bbox_inches='tight')
        print(f"Saved: config_{cfg}_analysis.png")

print("\n" + "="*80)
print("  Analysis complete!")
print("="*80)
