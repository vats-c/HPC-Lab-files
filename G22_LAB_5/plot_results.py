#!/usr/bin/env python3
"""
plot_results.py – Generate ALL required plots for HPC Assignment 5.

Usage:
  1. Run experiments and redirect output to text files:
       ./Experiment_01_Approach1 > Experiment_01_Approach1.txt
       ./Experiment_01_Approach2 > Experiment_01_Approach2.txt
       ./Experiment_02_Approach1 > Experiment_02_Approach1.txt
       ./Experiment_02_Approach2 > Experiment_02_Approach2.txt
       ./Experiment_02_no_insert_delete > Experiment_02_no_insert_delete.txt
       ./verify_distribution   (creates CSV files automatically)

  2. Then run this script:
       python plot_results.py
"""
import os, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

OUT_DIR = "plots"
os.makedirs(OUT_DIR, exist_ok=True)

def parse_csv_lines(filename, skip_header=2):
    """Parse CSV data from experiment output, skipping header lines.
    Handles UTF-16LE (PowerShell redirect) and UTF-8 encodings."""
    data = []
    for enc in ['utf-16', 'utf-8', 'latin-1']:
        try:
            with open(filename, encoding=enc) as f:
                lines = f.readlines()
            break
        except (UnicodeError, UnicodeDecodeError):
            continue
    else:
        print(f"  [ERROR] Cannot decode {filename}")
        return None
    for line in lines[skip_header:]:
        line = line.strip()
        if not line or 'OOM' in line:
            continue
        parts = line.split(',')
        try:
            data.append([float(x) for x in parts])
        except ValueError:
            continue
    return np.array(data) if data else None

# =========================================================================
# PLOT 1: Experiment 1 – Execution time vs particles (log-log)
# One plot per grid config, curves for Approach1 & Approach2
# =========================================================================
def plot_exp1():
    for tag, fname in [("Approach1", "Experiment_01_Approach1.txt"),
                       ("Approach2", "Experiment_01_Approach2.txt")]:
        if not os.path.exists(fname):
            print(f"  [SKIP] {fname} not found")
            continue
        data = parse_csv_lines(fname)
        if data is None:
            continue
        # cols: NX, NY, Particles, Interp, Mover, Total
        grids = sorted(set([(int(r[0]),int(r[1])) for r in data]))
        for nx, ny in grids:
            mask = (data[:,0]==nx) & (data[:,1]==ny)
            subset = data[mask]
            particles = subset[:,2]
            total = subset[:,5]
            plt.figure(figsize=(8,6))
            plt.loglog(particles, total, 'o-', linewidth=2, markersize=8,
                       label=f'{tag} ({nx}x{ny})')
            plt.xlabel('Number of Particles', fontsize=13)
            plt.ylabel('Total Execution Time (s)', fontsize=13)
            plt.title(f'Exp 1: Scaling – {tag} ({nx}×{ny})', fontsize=14)
            plt.legend(fontsize=12)
            plt.grid(True, which='both', alpha=0.3)
            plt.tight_layout()
            plt.savefig(f"{OUT_DIR}/exp1_{tag}_{nx}x{ny}.png", dpi=150)
            plt.close()
            print(f"  Saved exp1_{tag}_{nx}x{ny}.png")

    # Combined comparison plots (both approaches on same figure per grid)
    d_data = parse_csv_lines("Experiment_01_Approach1.txt") if os.path.exists("Experiment_01_Approach1.txt") else None
    i_data = parse_csv_lines("Experiment_01_Approach2.txt") if os.path.exists("Experiment_01_Approach2.txt") else None
    if d_data is not None and i_data is not None:
        grids = sorted(set([(int(r[0]),int(r[1])) for r in d_data]))
        for nx, ny in grids:
            plt.figure(figsize=(9,6))
            for label, dd, marker in [("Approach1 (Deferred)", d_data, 's-'),
                                       ("Approach2 (Immediate)", i_data, 'o-')]:
                mask = (dd[:,0]==nx) & (dd[:,1]==ny)
                s = dd[mask]
                plt.loglog(s[:,2], s[:,5], marker, linewidth=2, markersize=8, label=label)
            plt.xlabel('Number of Particles', fontsize=13)
            plt.ylabel('Total Execution Time (s)', fontsize=13)
            plt.title(f'Exp 1: Approach1 vs Approach2 ({nx}×{ny})', fontsize=14)
            plt.legend(fontsize=12)
            plt.grid(True, which='both', alpha=0.3)
            plt.tight_layout()
            plt.savefig(f"{OUT_DIR}/exp1_compare_{nx}x{ny}.png", dpi=150)
            plt.close()
            print(f"  Saved exp1_compare_{nx}x{ny}.png")

# =========================================================================
# PLOT 2: Experiment 2 – Speedup vs threads
# =========================================================================
def plot_exp2():
    files = {
        "Approach1 (with ins/del)":  "Experiment_02_Approach1.txt",
        "Approach2 (with ins/del)": "Experiment_02_Approach2.txt",
        "No ins/del (Assign-04)":   "Experiment_02_no_insert_delete.txt",
    }
    datasets = {}
    for label, fname in files.items():
        if os.path.exists(fname):
            d = parse_csv_lines(fname)
            if d is not None:
                datasets[label] = d

    if not datasets:
        print("  [SKIP] No exp2 data files found")
        return

    # Determine grids from first available dataset
    first = list(datasets.values())[0]
    grids = sorted(set([(int(r[0]),int(r[1])) for r in first]))

    for nx, ny in grids:
        plt.figure(figsize=(9,6))
        for label, dd in datasets.items():
            mask = (dd[:,0]==nx) & (dd[:,1]==ny)
            s = dd[mask]
            if len(s) == 0:
                continue
            threads = s[:,2].astype(int)
            # Speedup is col 5 for Approach1/Approach2, col 5 for no_insert
            speedup = s[:,5]
            plt.plot(threads, speedup, 'o-', linewidth=2, markersize=8, label=label)
        # Ideal speedup line
        max_t = 16
        plt.plot([2,4,8,16], [2,4,8,16], 'k--', alpha=0.4, label='Ideal')
        plt.xlabel('Number of Threads', fontsize=13)
        plt.ylabel('Speedup', fontsize=13)
        plt.title(f'Exp 2: Speedup vs Threads ({nx}×{ny}, 14M particles)', fontsize=14)
        plt.legend(fontsize=11)
        plt.grid(True, alpha=0.3)
        plt.xticks([2,4,8,16])
        plt.tight_layout()
        plt.savefig(f"{OUT_DIR}/exp2_speedup_{nx}x{ny}.png", dpi=150)
        plt.close()
        print(f"  Saved exp2_speedup_{nx}x{ny}.png")

# =========================================================================
# PLOT 3: Particle distribution verification
# =========================================================================
def plot_distribution():
    for fname, title in [("positions_initial.csv", "Initial"),
                          ("positions_after_Approach1.csv", "After Approach1 (Deferred) Mover"),
                          ("positions_after_Approach2.csv", "After Approach2 (Immediate) Mover")]:
        if not os.path.exists(fname):
            print(f"  [SKIP] {fname} not found")
            continue
        d = np.genfromtxt(fname, delimiter=',', skip_header=1)
        x, y = d[:,0], d[:,1]

        fig, axes = plt.subplots(1, 3, figsize=(16, 5))

        # Scatter plot (subsample for speed)
        idx = np.random.choice(len(x), min(5000, len(x)), replace=False)
        axes[0].scatter(x[idx], y[idx], s=0.5, alpha=0.5)
        axes[0].set_xlim(0,1); axes[0].set_ylim(0,1)
        axes[0].set_title(f'Positions – {title}')
        axes[0].set_xlabel('x'); axes[0].set_ylabel('y')
        axes[0].set_aspect('equal')

        # Histograms
        axes[1].hist(x, bins=50, alpha=0.7, color='steelblue', edgecolor='black')
        axes[1].set_title(f'X Distribution – {title}')
        axes[1].set_xlabel('x')

        axes[2].hist(y, bins=50, alpha=0.7, color='coral', edgecolor='black')
        axes[2].set_title(f'Y Distribution – {title}')
        axes[2].set_xlabel('y')

        plt.tight_layout()
        tag = fname.replace('.csv','').replace('positions_','')
        plt.savefig(f"{OUT_DIR}/distribution_{tag}.png", dpi=150)
        plt.close()
        print(f"  Saved distribution_{tag}.png")

# =========================================================================
# PLOT 4: Per-particle execution time vs PPC
# =========================================================================
def plot_ppc():
    """Per-particle time = Total / (NUM_Points * Maxiter), PPC = Particles / (NX*NY)."""
    for tag, fname in [("Approach1", "Experiment_01_Approach1.txt"),
                       ("Approach2", "Experiment_01_Approach2.txt")]:
        if not os.path.exists(fname):
            continue
        data = parse_csv_lines(fname)
        if data is None:
            continue
        grids = sorted(set([(int(r[0]),int(r[1])) for r in data]))
        plt.figure(figsize=(9,6))
        for nx, ny in grids:
            mask = (data[:,0]==nx) & (data[:,1]==ny)
            s = data[mask]
            ppc = s[:,2] / (nx * ny)
            per_particle = s[:,5] / (s[:,2] * 10)  # 10 iterations
            plt.loglog(ppc, per_particle, 'o-', linewidth=2, markersize=8,
                       label=f'{nx}×{ny}')
        plt.xlabel('Particles Per Cell (PPC)', fontsize=13)
        plt.ylabel('Per-Particle Execution Time (s)', fontsize=13)
        plt.title(f'Per-Particle Time vs PPC – {tag}', fontsize=14)
        plt.legend(fontsize=12)
        plt.grid(True, which='both', alpha=0.3)
        plt.tight_layout()
        plt.savefig(f"{OUT_DIR}/ppc_{tag}.png", dpi=150)
        plt.close()
        print(f"  Saved ppc_{tag}.png")

# =========================================================================
# TABLE: Memory and FLOP analysis
# =========================================================================
def print_memory_table():
    print("\n=== Memory & FLOP Analysis Table ===")
    print(f"{'NX':>6} {'NY':>6} {'Particles':>12} {'Points(MB)':>12} "
          f"{'Mesh(KB)':>10} {'Total(MB)':>10} {'FLOPs(G)':>10} {'PPC':>10}")
    grids = [(250,100),(500,200),(1000,400)]
    particles = [100, 10000, 1000000, 100000000, 1000000000]
    maxiter = 10
    for nx, ny in grids:
        for np_ in particles:
            pts_bytes = np_ * 16          # 2 doubles per particle
            mesh_bytes = (nx+1)*(ny+1)*8  # 1 double per node
            total_mb = (pts_bytes + mesh_bytes) / 1e6
            # FLOPs: interp ~24/particle + mover ~10/particle = ~34/particle/iter
            flops = np_ * maxiter * 34
            ppc = np_ / (nx * ny)
            print(f"{nx:>6} {ny:>6} {np_:>12} {pts_bytes/1e6:>12.2f} "
                  f"{mesh_bytes/1e3:>10.2f} {total_mb:>10.2f} "
                  f"{flops/1e9:>10.3f} {ppc:>10.1f}")

# =========================================================================
if __name__ == "__main__":
    print("Generating Experiment 1 plots...")
    plot_exp1()
    print("\nGenerating Experiment 2 plots...")
    plot_exp2()
    print("\nGenerating distribution plots...")
    plot_distribution()
    print("\nGenerating PPC plots...")
    plot_ppc()
    print_memory_table()
    print(f"\nAll plots saved to {OUT_DIR}/")
