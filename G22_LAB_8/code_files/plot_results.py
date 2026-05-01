import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

# Load benchmark results
try:
    df = pd.read_csv('benchmark_results.csv')
except FileNotFoundError:
    print("benchmark_results.csv not found. Please run benchmarks first.")
    exit()

configs = ['a', 'b', 'c', 'd', 'e']
core_counts = [2, 4, 8, 16, 32, 64]

sns.set_theme(style="whitegrid")

# 1. Execution Time vs Cores
plt.figure(figsize=(10, 6))
for conf in configs:
    subset = df[df['Config'] == conf]
    # Filter for hybrid runs (Cores > 1)
    subset_hybrid = subset[subset['Cores'] > 1].sort_values('Cores')
    if not subset_hybrid.empty:
        plt.plot(subset_hybrid['Cores'], subset_hybrid['Total_Time'], marker='o', label=f"Config {conf}")

plt.xscale('log', base=2)
plt.xticks(core_counts, core_counts)
plt.xlabel('Number of Cores')
plt.ylabel('Execution Time (seconds)')
plt.title('Execution Time vs Cores for Different Configurations')
plt.legend()
plt.tight_layout()
plt.savefig('execution_time_vs_cores.png')
plt.close()

# 2. Speedup vs Cores
plt.figure(figsize=(10, 6))
for conf in configs:
    subset = df[df['Config'] == conf]
    
    # Get serial baseline (Cores == 1)
    serial_row = subset[subset['Cores'] == 1]
    if serial_row.empty:
        continue
    t_serial = serial_row['Total_Time'].values[0]
    
    # Filter for hybrid runs
    subset_hybrid = subset[subset['Cores'] > 1].sort_values('Cores')
    if not subset_hybrid.empty:
        speedup = t_serial / subset_hybrid['Total_Time']
        plt.plot(subset_hybrid['Cores'], speedup, marker='o', label=f"Config {conf}")

# Ideal speedup line
plt.plot(core_counts, core_counts, 'k--', label='Ideal Speedup')

plt.xscale('log', base=2)
plt.yscale('log', base=2)
plt.xticks(core_counts, core_counts)
plt.yticks(core_counts, core_counts)
plt.xlabel('Number of Cores')
plt.ylabel('Speedup (T_serial / T_parallel)')
plt.title('Speedup vs Cores for Different Configurations')
plt.legend()
plt.tight_layout()
plt.savefig('speedup_vs_cores.png')
plt.close()

# 3. Phase Breakdown (Stacked Bar Chart for Config 'd' at 16 cores, as example)
# Or we can do a breakdown of scaling for Config 'd'
plt.figure(figsize=(10, 6))
conf_d = df[df['Config'] == 'd'].sort_values('Cores')
if not conf_d.empty:
    cores = conf_d['Cores'].astype(str).tolist()
    interp = conf_d['Interp_Time'].values
    comm = conf_d['Comm_Time'].values
    norm = conf_d['Norm_Time'].values
    mover = conf_d['Mover_Time'].values
    
    fig, ax = plt.subplots(figsize=(10, 6))
    ax.bar(cores, interp, label='Interpolation')
    ax.bar(cores, comm, bottom=interp, label='MPI Comm')
    ax.bar(cores, norm, bottom=interp+comm, label='Normalization')
    ax.bar(cores, mover, bottom=interp+comm+norm, label='Mover')
    
    ax.set_xlabel('Cores')
    ax.set_ylabel('Execution Time (seconds)')
    ax.set_title("Phase Breakdown vs Cores (Config 'd')")
    ax.legend()
    plt.tight_layout()
    plt.savefig('phase_breakdown_config_d.png')
    plt.close()

print("Plots generated successfully: execution_time_vs_cores.png, speedup_vs_cores.png, phase_breakdown_config_d.png")
