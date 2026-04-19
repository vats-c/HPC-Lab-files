#!/usr/bin/env python3
"""
HPC Assignment 07 - Plotting & Analysis Script
Generates:
  1. Speedup vs Cores (all configs)
  2. Execution Time vs Cores (all configs)
  3. Parallel Efficiency vs Cores
  4. Per-config analysis (time + speedup side-by-side)
  5. Phase breakdown bar chart
"""

import csv
import os
import sys

try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("WARNING: matplotlib not found. Will generate tables but no plots.")

# --- Load data ---
csv_file = "benchmark_results.csv"
if not os.path.exists(csv_file):
    print(f"Error: {csv_file} not found. Run run_benchmarks.py first.")
    sys.exit(1)

data = {}           # data[config] = {threads: {interp, norm, mover, denorm, total, voids}}
config_info = {}    # config_info[config] = (Nx, Ny, Points)

with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        cfg = row['Config']
        threads = int(row['Threads'])
        info = {
            'interp': float(row['Interp_Time']),
            'norm':   float(row['Norm_Time']),
            'mover':  float(row['Mover_Time']),
            'denorm': float(row['Denorm_Time']),
            'total':  float(row['Total_Time']),
            'voids':  int(row['Voids']),
        }
        if cfg not in data:
            data[cfg] = {}
            config_info[cfg] = (int(row['Nx']), int(row['Ny']), int(row['Points']))
        data[cfg][threads] = info

# --- Compute speedup and efficiency ---
print("\n" + "=" * 85)
print("  HPC Assignment 07 - Performance Analysis")
print("=" * 85)

configs_sorted = sorted(data.keys())
all_threads = sorted(set(t for cfg_data in data.values() for t in cfg_data.keys()))

# Execution Times
print(f"\n--- Total Execution Times (seconds) ---")
print(f"  {'Config':<8} {'Nx':<6} {'Ny':<6} {'Points':<12}", end="")
for t in all_threads:
    print(f"  T={t:<4}", end="")
print()
print("-" * (40 + 10 * len(all_threads)))

for cfg in configs_sorted:
    nx, ny, pts = config_info[cfg]
    print(f"  {cfg:<8} {nx:<6} {ny:<6} {pts:<12}", end="")
    for t in all_threads:
        if t in data[cfg]:
            print(f"  {data[cfg][t]['total']:<8.4f}", end="")
        else:
            print(f"  {'N/A':<8}", end="")
    print()

# Phase Breakdown
print(f"\n--- Phase Breakdown (1 thread / serial) ---")
print(f"  {'Config':<8} {'Interp':<12} {'Norm':<12} {'Mover':<12} {'Denorm':<12} {'Total':<12} {'Voids':<10}")
for cfg in configs_sorted:
    if 1 in data[cfg]:
        d = data[cfg][1]
        print(f"  {cfg:<8} {d['interp']:<12.4f} {d['norm']:<12.4f} {d['mover']:<12.4f} {d['denorm']:<12.4f} {d['total']:<12.4f} {d['voids']:<10}")

# Speedup
print(f"\n--- Speedup (Serial_Time / Parallel_Time) ---")
speedup_data = {}
for cfg in configs_sorted:
    serial_time = data[cfg].get(1, {}).get('total', None)
    speedup_data[cfg] = {}
    nx, ny, pts = config_info[cfg]
    print(f"  {cfg:<8} {nx:<6} {ny:<6} {pts:<12}", end="")
    for t in all_threads:
        if serial_time and t in data[cfg]:
            s = serial_time / data[cfg][t]['total'] if data[cfg][t]['total'] > 0 else 0
            speedup_data[cfg][t] = s
            print(f"  {s:<8.2f}", end="")
        else:
            print(f"  {'N/A':<8}", end="")
    print()

# Parallel Efficiency
print(f"\n--- Parallel Efficiency (Speedup / Threads × 100%) ---")
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
print(f"\n--- Maximum Speedup Achieved ---")
for cfg in configs_sorted:
    nx, ny, pts = config_info[cfg]
    if speedup_data[cfg]:
        max_s = max(speedup_data[cfg].values())
        max_t = max(speedup_data[cfg], key=speedup_data[cfg].get)
        print(f"  Config {cfg} (Nx={nx}, Ny={ny}, Pts={pts:,}): {max_s:.2f}x at {max_t} threads")

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
        threads_list = sorted(t for t in speedup_data[cfg] if t > 1)
        speedups = [speedup_data[cfg][t] for t in threads_list]
        if threads_list:
            ax.plot(threads_list, speedups, marker=markers[i], color=colors[i],
                    linewidth=2, markersize=8, label=config_labels[cfg])
    ax.plot(all_threads, all_threads, 'k--', linewidth=1, alpha=0.5, label='Ideal Speedup')
    ax.set_xlabel('Number of Threads', fontsize=13)
    ax.set_ylabel('Speedup', fontsize=13)
    ax.set_title('Speedup vs Number of Threads', fontsize=15, fontweight='bold')
    ax.set_xticks([t for t in all_threads if t > 1])
    ax.legend(fontsize=10, loc='upper left')
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig('speedup_vs_cores.png', dpi=150, bbox_inches='tight')
    print("\nSaved: speedup_vs_cores.png")

    # ---- Plot 2: Execution Time vs Cores ----
    fig, ax = plt.subplots(figsize=(10, 7))
    for i, cfg in enumerate(configs_sorted):
        threads_list = sorted(data[cfg].keys())
        times = [data[cfg][t]['total'] for t in threads_list]
        ax.plot(threads_list, times, marker=markers[i], color=colors[i],
                linewidth=2, markersize=8, label=config_labels[cfg])
    ax.set_xlabel('Number of Threads', fontsize=13)
    ax.set_ylabel('Execution Time (seconds)', fontsize=13)
    ax.set_title('Execution Time vs Number of Threads', fontsize=15, fontweight='bold')
    ax.set_xticks(all_threads)
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig('execution_time_vs_cores.png', dpi=150, bbox_inches='tight')
    print("Saved: execution_time_vs_cores.png")

    # ---- Plot 3: Parallel Efficiency vs Cores ----
    fig, ax = plt.subplots(figsize=(10, 7))
    for i, cfg in enumerate(configs_sorted):
        threads_list = sorted(t for t in speedup_data[cfg] if t > 1)
        efficiency = [(speedup_data[cfg][t] / t) * 100 for t in threads_list]
        if threads_list:
            ax.plot(threads_list, efficiency, marker=markers[i], color=colors[i],
                    linewidth=2, markersize=8, label=config_labels[cfg])
    ax.axhline(y=100, color='k', linestyle='--', linewidth=1, alpha=0.5, label='Ideal (100%)')
    ax.set_xlabel('Number of Threads', fontsize=13)
    ax.set_ylabel('Parallel Efficiency (%)', fontsize=13)
    ax.set_title('Parallel Efficiency vs Number of Threads', fontsize=15, fontweight='bold')
    ax.set_xticks([t for t in all_threads if t > 1])
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 120)
    plt.tight_layout()
    plt.savefig('parallel_efficiency_vs_cores.png', dpi=150, bbox_inches='tight')
    print("Saved: parallel_efficiency_vs_cores.png")

    # ---- Plot 4: Per-config analysis (time + speedup side-by-side) ----
    for i, cfg in enumerate(configs_sorted):
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
        nx, ny, pts = config_info[cfg]

        threads_list = sorted(data[cfg].keys())
        times = [data[cfg][t]['total'] for t in threads_list]
        speedups = [speedup_data[cfg].get(t, 1.0) for t in threads_list]

        ax1.plot(threads_list, times, marker='o', color=colors[i], linewidth=2, markersize=8)
        ax1.set_xlabel('Number of Threads', fontsize=12)
        ax1.set_ylabel('Execution Time (seconds)', fontsize=12)
        ax1.set_title(f'Execution Time - {config_labels[cfg]}', fontsize=12, fontweight='bold')
        ax1.set_xticks(all_threads)
        ax1.grid(True, alpha=0.3)

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

    # ---- Plot 5: Phase breakdown (stacked bar for serial) ----
    fig, ax = plt.subplots(figsize=(12, 6))
    cfgs_with_serial = [c for c in configs_sorted if 1 in data[c]]
    x = range(len(cfgs_with_serial))
    bar_w = 0.6

    interp_times = [data[c][1]['interp'] for c in cfgs_with_serial]
    mover_times  = [data[c][1]['mover']  for c in cfgs_with_serial]
    norm_times   = [data[c][1]['norm']   for c in cfgs_with_serial]
    denorm_times = [data[c][1]['denorm'] for c in cfgs_with_serial]

    ax.bar(x, interp_times, bar_w, label='Interpolation', color='#2196F3')
    ax.bar(x, mover_times,  bar_w, bottom=interp_times, label='Mover', color='#FF5722')
    bottom2 = [a+b for a,b in zip(interp_times, mover_times)]
    ax.bar(x, norm_times,   bar_w, bottom=bottom2, label='Normalization', color='#4CAF50')
    bottom3 = [a+b for a,b in zip(bottom2, norm_times)]
    ax.bar(x, denorm_times, bar_w, bottom=bottom3, label='Denormalization', color='#9C27B0')

    labels = [config_labels[c] for c in cfgs_with_serial]
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=15, ha='right', fontsize=9)
    ax.set_ylabel('Time (seconds)', fontsize=12)
    ax.set_title('Phase Breakdown (Serial Baseline)', fontsize=14, fontweight='bold')
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3, axis='y')
    plt.tight_layout()
    plt.savefig('phase_breakdown.png', dpi=150, bbox_inches='tight')
    print("Saved: phase_breakdown.png")

print("\n" + "=" * 85)
print("  Analysis complete!")
print("=" * 85)
