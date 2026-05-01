import os
import subprocess
import csv
import platform

# Cross-platform configurations
ext = ".exe" if platform.system() == "Windows" else ".out"
mpi_cmd = r'"C:\Program Files\Microsoft MPI\Bin\mpiexec.exe"' if platform.system() == "Windows" else "mpirun"
mpi_flag = "-n" if platform.system() == "Windows" else "-np"

configs = [
    {'name': 'a', 'nx': 250, 'ny': 100, 'points': 900000, 'maxiter': 10},
    {'name': 'b', 'nx': 250, 'ny': 100, 'points': 5000000, 'maxiter': 10},
    {'name': 'c', 'nx': 500, 'ny': 200, 'points': 3600000, 'maxiter': 10},
    {'name': 'd', 'nx': 500, 'ny': 200, 'points': 20000000, 'maxiter': 10},
    {'name': 'e', 'nx': 1000, 'ny': 400, 'points': 14000000, 'maxiter': 10}
]

# We want cores from 2 to 64 (2, 4, 8, 16, 32, 64)
# Let's define the combinations of (ranks, threads) to achieve these core counts
core_configs = [
    (1, 1), # 1 core baseline
    (1, 2), # 2 cores
    (1, 4), # 4 cores
    (2, 4), # 8 cores
    (4, 4), # 16 cores
    (4, 8), # 32 cores
    (4, 16) # 64 cores
]

csv_file = 'benchmark_results.csv'

with open(csv_file, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow(['Config', 'Nx', 'Ny', 'Points', 'Maxiter', 'Ranks', 'Threads', 'Cores', 'Total_Time', 'Interp_Time', 'Comm_Time', 'Norm_Time', 'Mover_Time'])

for config in configs:
    print(f"\n--- Running Config {config['name']} ---")
    
    # Generate input file
    prefix = "" if platform.system() == "Windows" else "./"
    cmd_gen = f"{prefix}input_maker{ext} {config['nx']} {config['ny']} {config['points']} {config['maxiter']}"
    subprocess.run(cmd_gen, shell=True)
    
    # Run Serial Baseline
    print("Running Serial Baseline...")
    result = subprocess.run(f"{prefix}serial{ext} input.bin", shell=True, capture_output=True, text=True)
    serial_time = 0.0
    for line in result.stdout.split('\n'):
        if "Total time:" in line:
            serial_time = float(line.split()[2])
    
    with open(csv_file, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([config['name'], config['nx'], config['ny'], config['points'], config['maxiter'], 1, 1, 1, serial_time, 0, 0, 0, 0])
    
    # Run Hybrid
    for ranks, threads in core_configs:
        cores = ranks * threads
        print(f"Running Hybrid with {ranks} ranks, {threads} threads ({cores} cores)...")
        os.environ["OMP_NUM_THREADS"] = str(threads)
        
        cmd_run = f"{mpi_cmd} {mpi_flag} {ranks} {prefix}hybrid{ext} input.bin"
        
        result = subprocess.run(cmd_run, shell=True, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"ERROR: Hybrid run failed!\n{result.stderr}\n{result.stdout}")
        
        total_time = 0.0
        interp_time = 0.0
        comm_time = 0.0
        norm_time = 0.0
        mover_time = 0.0
        
        for line in result.stdout.split('\n'):
            if "Total Time" in line: total_time = float(line.split(':')[1].strip().split()[0])
            if "Interpolation:" in line: interp_time = float(line.split(':')[1].strip().split()[0])
            if "MPI Comm" in line: comm_time = float(line.split(':')[1].strip().split()[0])
            if "Normalization:" in line: norm_time = float(line.split(':')[1].strip().split()[0])
            if "Mover" in line: mover_time = float(line.split(':')[1].strip().split()[0])
            
        with open(csv_file, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([config['name'], config['nx'], config['ny'], config['points'], config['maxiter'], ranks, threads, cores, total_time, interp_time, comm_time, norm_time, mover_time])

print("\nBenchmarking complete! Results saved to benchmark_results.csv")
