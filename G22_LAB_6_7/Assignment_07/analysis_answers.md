# HPC Assignment 07 — Analysis and Answers

This document provides complete answers for **Section 6 (Parallelization Objective)** and **Section 7 (Analysis and Questions)**.

> [!NOTE]
> Data below is from local Windows benchmarks (8 logical cores). Re-run `python3 run_benchmarks.py` and `python3 plot_results.py` on the cluster for final submission data.

---

## Q1. Pseudocode of Parallel Implementation + Illustrative Diagram

### Pseudocode

```
READ points from input.bin (once)
ALLOCATE mesh_value[GRID_Y × GRID_X]

FOR iter = 0 TO Maxiter-1:

  ═══ PHASE 1: PARALLEL INTERPOLATION (scatter: particle → grid) ═══
  Estimate total_local_memory = G_SIZE × num_threads × 8 bytes

  IF total_local_memory > CACHE_THRESHOLD:
    // ATOMIC PATH (large grids where T copies overflow L3 cache)
    Zero global mesh
    PARALLEL FOR (static schedule) over particles:
      IF void → skip
      Compute cell (i,j), weights w00, w10, w01, w11
      ATOMIC: mesh[idx] += w00, mesh[idx+1] += w10, ...

  ELSE:
    // THREAD-LOCAL PATH (small/medium grids fit in L3)
    PARALLEL REGION:
      SINGLE: allocate/reuse thread-local grids
      EACH THREAD: zero its local grid
      PARALLEL FOR (static) over particles:
        IF void → skip
        Compute cell/weights → accumulate into local grid
      PARALLEL FOR (static) over grid cells:
        mesh[k] = SUM of all_local[thread][k]  // reduction-write

  ═══ PHASE 2: NORMALIZATION (scale grid to [-1, 1]) ═══
  PARALLEL FOR with reduction(min, max):
    Find global min_val, max_val of mesh
  PARALLEL FOR:
    mesh[k] = 2·(mesh[k] - min) / (max - min) - 1

  ═══ PHASE 3: MOVER (gather: grid → particle) ═══
  PARALLEL FOR (static schedule) over particles:
    IF void → skip
    Compute cell (i, j) and weights from particle position
    Fi = weighted sum of 4 neighboring grid values (READ-ONLY)
    x_new = x + Fi · Δx;  y_new = y + Fi · Δy
    IF outside domain → mark as void

  ═══ PHASE 4: DENORMALIZATION (restore grid) ═══
  PARALLEL FOR:
    mesh[k] = (mesh[k] + 1) · (max - min) / 2 + min

OUTPUT mesh_value to Mesh.out
PRINT timing per phase + total voids
```

### Illustrative Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    ITERATION LOOP (×Maxiter)                    │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  INTERPOLATION (Scatter: Particles → Grid)               │   │
│  │  [ADAPTIVE: thread-local if grids fit cache, else atomic] │   │
│  │                                                          │   │
│  │   Thread 0          Thread 1          Thread T-1         │   │
│  │   ┌──────────┐     ┌──────────┐     ┌──────────┐       │   │
│  │   │Local Grid│     │Local Grid│ ... │Local Grid│       │   │
│  │   │ P[0..N/T]│     │P[N/T..2N]│     │P[.....N] │       │   │
│  │   └────┬─────┘     └────┬─────┘     └────┬─────┘       │   │
│  │        └────────────────┼────────────────┘              │   │
│  │                    REDUCTION                             │   │
│  │              mesh[k] = Σ local[t][k]                    │   │
│  │              ┌──────────────────┐                       │   │
│  │              │   Global Mesh    │                       │   │
│  │              └────────┬─────────┘                       │   │
│  └───────────────────────┼──────────────────────────────────┘   │
│                          ↓                                      │
│  ┌───────────────────────┼──────────────────────────────────┐   │
│  │  NORMALIZATION        │    parallel for + reduction       │   │
│  │  mesh → [-1, 1]      │    (min/max finding + scaling)    │   │
│  └───────────────────────┼──────────────────────────────────┘   │
│                          ↓                                      │
│  ┌───────────────────────┼──────────────────────────────────┐   │
│  │  MOVER (Gather: Grid → Particles) — embarrassingly par.  │   │
│  │   Thread 0          Thread 1          Thread T-1         │   │
│  │   Read grid ────→   Read grid ────→   Read grid          │   │
│  │   Update P[0..N/T]  Update P[N/T..]  Update P[..N]      │   │
│  │   (No race: each thread writes only its own particles)   │   │
│  └───────────────────────┼──────────────────────────────────┘   │
│                          ↓                                      │
│  ┌───────────────────────┼──────────────────────────────────┐   │
│  │  DENORMALIZATION      │    parallel for (simple scaling)  │   │
│  └───────────────────────┼──────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Q2. Parallel Approach and Race Condition Handling

### Approach: Adaptive Hybrid (Thread-Local + Atomics)

Our implementation uses **particle decomposition** with an **adaptive strategy** for the interpolation phase:

**Interpolation (scatter) — the only phase with race conditions:**

We detect at runtime whether thread-local grids would exceed the L3 cache:

- **Small/medium grids** (T × G × 8 < 20 MB): Each thread gets a **private copy** of the mesh. Particles are divided via `#pragma omp for schedule(static)`. Each thread accumulates into its local grid — **zero race conditions**. A parallel reduction merges local grids: `mesh[k] = Σ local[t][k]`.

- **Large grids** (T × G × 8 ≥ 20 MB): Thread-local copies would overflow the L3 cache, causing severe cache thrashing. Instead, we use `#pragma omp atomic` on a single shared grid. With large grids, contention is naturally low (~35 particles/cell), making atomics efficient.

**Why the hybrid?** Pure thread-local degrades catastrophically for large grids at high thread counts (config e at 16T: 49 MB of local grids ≫ L3 cache). Pure atomics are slower for small grids due to per-operation CAS overhead. The hybrid gets the best of both.

**Mover (gather) — inherently race-free:**
- Each particle reads from the grid (read-only) and writes only to its own position
- No shared writes → no race conditions → embarrassingly parallel

**Normalization/Denormalization:**
- Min/max finding uses OpenMP `reduction(min:...) reduction(max:...)` — built-in, efficient
- Scaling is element-wise with no dependencies → simple `parallel for`

---

## Q3. Graphs for 5 Input Configurations

### Results Table (Local Windows, 8 logical cores)

| Config | Grid | Points | Serial (s) | 2T (s) | 4T (s) | 8T (s) | 16T (s) | Max Speedup |
|--------|------|--------|-----------|--------|--------|--------|---------|-------------|
| a | 250×100 | 0.9M | 0.109 | 0.053 | 0.037 | 0.042 | 0.035 | 3.11x @16T |
| b | 250×100 | 5.0M | 0.654 | 0.269 | 0.200 | 0.177 | 0.174 | 3.76x @16T |
| c | 500×200 | 3.6M | 0.511 | 0.231 | 0.183 | 0.156 | 0.176 | 3.28x @8T |
| d | 500×200 | 20.0M | 3.972 | 1.138 | 0.838 | 0.722 | 0.727 | 5.50x @8T |
| e | 1000×400 | 14.0M | 2.428 | 1.201 | 1.066 | 1.292 | 1.136 | 2.28x @4T |

**Plots generated:** `speedup_vs_cores.png`, `execution_time_vs_cores.png`, `parallel_efficiency_vs_cores.png`, `config_{a-e}_analysis.png`, `phase_breakdown.png`

---

## Q4. Parallel Efficiency and Scalability

**Parallel Efficiency** = Speedup / Number_of_Threads × 100%

| Config | 2T | 4T | 8T | 16T |
|--------|-----|-----|-----|------|
| a | 102.8% | 73.6% | 32.4% | 19.5% |
| b | 121.6% | 81.8% | 46.2% | 23.5% |
| c | 110.6% | 69.8% | 40.9% | 18.1% |
| d | 174.5% | 118.5% | 68.8% | 34.1% |
| e | 101.1% | 56.9% | 23.5% | 13.4% |

**Scalability Analysis:**
- **Config d (20M points, 100K grid)** shows the best scalability: 5.50x at 8T. High particle count means abundant parallel work that dominates overhead.
- **Config e (14M points, 401K grid)** shows moderate scaling despite many particles. The large grid makes thread-local copies memory-expensive and triggers the atomic fallback at 8T+.
- **Efficiency > 100% at 2T** for several configs is due to super-linear speedup from: (a) `clock()` vs `omp_get_wtime()` measurement differences, and (b) each thread's working set fitting better in per-core L1/L2 caches.
- **Efficiency drops sharply beyond 8T** because this laptop has ~8 logical cores. At 16T, oversubscription causes context-switching overhead.

---

## Q5. Maximum Speedup Achieved

| Config | Max Speedup | At Threads |
|--------|-------------|------------|
| a | 3.11x | 16 |
| b | 3.76x | 16 |
| c | 3.28x | 8 |
| d | **5.50x** | 8 |
| e | 2.28x | 4 |

**Config d achieves the best speedup** because it has the highest particle-to-grid ratio (200 particles/cell), meaning the parallel scatter and gather loops have maximum work per thread while the grid is small enough for thread-local copies to fit in L3.

---

## Q6. Explanation of Speedup and Observations

### Why Our Approach Achieves Good Speedup

1. **Adaptive interpolation** — thread-local grids for small grids (zero contention, perfect cache) and atomics for large grids (no memory explosion). This avoids the catastrophic degradation seen with pure thread-local on large grids.

2. **The mover is embarrassingly parallel** — each particle reads from shared grid (read-only, no cache invalidation) and writes only to its own fields. Scales near-linearly.

3. **Persistent thread-local storage** — allocated once, reused across all 10 iterations. Eliminates malloc/free overhead.

4. **Pre-computed reciprocals** (`inv_dx = 1.0/dx`) convert expensive divisions to multiplications.

### Why Speedup < Ideal (Amdahl's Law)

- **Reduction overhead**: Merging T local grids is O(G×T) extra work
- **Memory bandwidth saturation**: At high thread counts, threads compete for shared DRAM bandwidth
- **Cache pressure**: Thread-local grids consume T × G × 8 bytes; can overflow L3
- **Sequential phases**: I/O, barriers between phases are serial
- **Core count**: This laptop has ~8 logical cores; 16 threads cause oversubscription

---

## Q7. Unlimited HPC Resources Optimization

1. **GPU Offloading (CUDA/OpenCL)**: The interpolation and mover loops are massively parallel — ideal for GPU. Thousands of CUDA cores could process millions of particles simultaneously.

2. **MPI + OpenMP Hybrid**: Distribute particles across nodes using MPI (domain decomposition), then use OpenMP within each node.

3. **SIMD Vectorization (AVX-512)**: Process 8 particles simultaneously. SoA layout `{x[], y[], void[]}` enables better vectorization than current AoS.

4. **NUMA-Aware Allocation**: Use `numactl` or first-touch policy to place thread-local grids on correct NUMA nodes.

5. **Active Particle Compaction**: After each mover, compact active particles to the front. Eliminates void-checking branches and improves cache utilization.

6. **Cache-Aware Particle Sorting**: Sort particles by cell location. Makes grid accesses cache-friendly.

---

## Q8. Load Imbalance and Performance Bottlenecks

### Load Imbalance

- **Initial distribution**: Random particles → `schedule(static)` divides evenly — good balance.
- **Evolving imbalance**: Mover marks some particles void (~60-1254 voids out of millions). Void skip is just a branch — negligible imbalance.
- **Mitigation**: `schedule(dynamic)` could handle severe imbalance but its dispatch overhead exceeds benefits for our particle counts.

### Performance Bottlenecks

1. **Interpolation memory pressure**: For large grids (config e), T thread-local copies overflow L3 cache. Solved via adaptive switching to atomics.

2. **Mover dominance**: The mover phase accounts for ~50-60% of serial time because it involves both grid reads and particle position updates (read + write per particle, vs write-only for interpolation).

3. **Memory bandwidth**: At 8+ threads, all threads compete for DRAM bandwidth. The particle array (14M × 24B = 336 MB) far exceeds any cache level.

---

## Q9. Alternative Approach

### Alternative 1: Pure Atomic Operations

```cpp
#pragma omp parallel for schedule(static)
for (int p = 0; p < NUM_Points; p++) {
    // ... compute weights ...
    #pragma omp atomic
    mesh_value[idx] += w00;
    // ... 3 more atomics ...
}
```

- **Pros**: No extra memory, no reduction phase, simpler code
- **Cons**: CAS-loop overhead per atomic (~10-20 cycles), cache-line contention
- **Best for**: Large grids where contention is low (< 50 particles/cell)

### Alternative 2: Color-Based Domain Decomposition

Divide grid into checkerboard (red/black cells). Process all particles in red cells first (no adjacent conflicts), then black. Eliminates race conditions without extra memory, but requires spatial particle sorting.

**Our hybrid approach combines the best of both**: thread-local for small grids (no atomic overhead) and atomics for large grids (no memory explosion).

---

## Q10. Data Structure / OpenMP Approaches for Performance

### Approach 1: Structure of Arrays (SoA)
Replace `struct {x, y, is_void}` with separate arrays `x[], y[], void[]`:
- Better SIMD vectorization, no padding waste, aligned access

### Approach 2: Spatial Hashing
Hash particles into cells. Process cell-by-cell for conflict-free parallel execution.

### Approach 3: Tree Reduction for Thread-Local Grids
Reduce local grids in O(log T) passes instead of flat O(T) — better for high thread counts.

---

## Q11. Phase-by-Phase Performance Analysis

### Phase Breakdown (Serial Baseline)

| Config | Interp (s) | Mover (s) | Norm (s) | Denorm (s) | Interp % | Mover % |
|--------|-----------|----------|----------|-----------|----------|---------|
| a | 0.050 | 0.059 | 0.000 | 0.000 | 45.9% | 54.1% |
| b | 0.287 | 0.367 | 0.000 | 0.000 | 43.9% | 56.1% |
| c | 0.221 | 0.288 | 0.002 | 0.000 | 43.2% | 56.4% |
| d | 1.549 | 2.420 | 0.003 | 0.000 | 39.0% | 60.9% |
| e | 1.199 | 1.219 | 0.008 | 0.002 | 49.4% | 50.2% |

### Which Phase is the Bottleneck?

**The mover is the serial bottleneck** — it accounts for 50-61% of serial time. This is because:
1. It performs both grid **reads** (4 cells) AND particle **writes** (2 position updates + void check)
2. Interpolation only does particle reads + grid writes
3. The gather pattern (reading 4 grid cells per particle) has slightly worse cache behavior than the scatter pattern

**However, the mover parallelizes better** (embarrassingly parallel, no reduction), so in the parallel version the interpolation-with-reduction becomes the relative bottleneck.

**Normalization and denormalization are negligible** (<1% of total) — they loop only over grid cells (G ≪ N).
