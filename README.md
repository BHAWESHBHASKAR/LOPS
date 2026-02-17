# LOPS — Lipschitz-Optimized Potential Search

**Part of the PHOTON Shortest Path Engine**

A math-driven shortest path algorithm that constructs multiple global **1-Lipschitz potentials** and uses their maximum difference as an admissible, consistent A\* heuristic — no coordinates required.

```
    ██╗      ██████╗ ██████╗ ███████╗
    ██║     ██╔═══██╗██╔══██╗██╔════╝
    ██║     ██║   ██║██████╔╝███████╗
    ██║     ██║   ██║██╔═══╝ ╚════██║
    ███████╗╚██████╔╝██║     ███████║
    ╚══════╝ ╚═════╝ ╚═╝     ╚══════╝

      Lipschitz-Optimized Potential Search
```

---

## The Core Idea

LOPS builds potentials via multi-source Dijkstra with random anchor labels:

```
phi(v) = min_i ( label_i + dist(anchor_i, v) )
```

Each potential `phi` is **1-Lipschitz**, meaning:

```
|phi(u) - phi(v)| <= dist(u, v)
```

The heuristic for A\* search is then:

```
h(v, t) = max_i |phi_i(v) - phi_i(t)|
```

This heuristic is **admissible** (never overestimates) and **consistent** (satisfies the triangle inequality), guaranteeing optimal shortest paths when used with standard A\*.

### Why LOPS Works

1. **Orthogonal to Landmarks** — ALT-style landmarks excel on geometric graphs (roads, grids). LOPS captures complementary structure in non-geometric graphs (social, random, scale-free).
2. **Combined Heuristic** — `max(h_ALT, h_LIPS)` preserves admissibility while capturing both geometric and topological structure.
3. **Degree-Weighted Anchors** — Anchors are sampled proportional to `(log2(deg+1) + 1)^2`, biasing toward high-degree hubs for better coverage.
4. **Auto-Tuning** — Number of potentials, anchors, landmarks, and label ranges all auto-tune based on graph size and degree CV.
5. **No Coordinates Needed** — Unlike A\* with Euclidean heuristics, LOPS works on any graph topology.

---

## Algorithm Variants

### LIPS-Exact
- Standard A\* search with LOPS heuristic (weight = 1.0)
- **Optimality**: 100% guaranteed
- Best for: When optimality is critical

### LIPS-Weighted
- Weighted A\* with inflated heuristic (default weight = 1.3)
- Finds paths faster at the cost of slight suboptimality
- Best for: When speed is the priority

### LIPS-Hybrid
- Runs weighted search first to get an upper bound, then exact search with pruning
- Combines speed of weighted search with optimality guarantees
- Best for: Best-of-both-worlds approach

### Bidirectional Mode
- All variants support bidirectional search from source and target simultaneously
- Terminates when lower bounds prove optimality
- Configurable per query type (exact vs weighted)

---

## Key Findings

From the budget-matched ablation study comparing ALT-only, LIPS-only, and Combined heuristics under identical preprocessing wall-time budgets:

| Graph Type | ALT Dominates? | LIPS Dominates? | Best Strategy |
|---|---|---|---|
| **Road Networks** | Yes | No | ALT or Combined |
| **Grid Graphs** | Yes | No | ALT or Combined |
| **Random Graphs** | No | Yes | LIPS or Combined |
| **Scale-Free Networks** | No | Partial | Combined |
| **Social Networks** | No | Partial | Combined |

**Key takeaway**: The Combined heuristic (`max(h_ALT, h_LIPS)`) is consistently near-best across all graph families, adapting to either regime without sacrificing admissibility.

---

## Theoretical Foundation

### Admissibility Proof

**Theorem**: The LOPS heuristic `h(v, t) = max_i |phi_i(v) - phi_i(t)|` is admissible and consistent.

**Proof**:
1. Each potential `phi_i` is 1-Lipschitz: `|phi_i(u) - phi_i(v)| <= dist(u, v)` for all `u, v`.
2. Therefore, for any potential `i`: `|phi_i(v) - phi_i(t)| <= dist(v, t)`.
3. The maximum over admissible bounds is itself admissible: `h(v, t) = max_i |phi_i(v) - phi_i(t)| <= dist(v, t)`.
4. Consistency follows from the triangle inequality on each `phi_i`.

### Complexity

| Operation | Time Complexity | Space Complexity |
|---|---|---|
| Preprocessing | O(K · (V + E) log V) | O(K · V) |
| Query | O((V + E) log V) | O(V) |

Where `K` = number of potentials + landmarks (auto-tuned, typically 8–24).

---

## Getting Started

### Requirements
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- (Optional) OpenMP, Intel TBB

### Build

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Run

```bash
# Main engine
./photon

# Comprehensive benchmark
./photon_bench

# LIPS budget-matched ablation study
./lips_ablation

# Algorithm showdown (LIPS vs DASH vs Dijkstra vs Bidirectional)
./algorithm_showdown

# Real-world SNAP dataset benchmark
./real_world_benchmark
```

### Usage Example

```cpp
#include <photon/lops.hpp>
#include <photon/graph.hpp>

int main() {
    using namespace photon;

    // Build a graph
    GraphBuilder builder(1000);
    builder.add_edge(0, 1, 1.0f);
    builder.add_edge(1, 2, 2.0f);
    // ... add more edges
    Graph graph = builder.build();

    // Initialize LOPS
    lops::LOPS solver;
    solver.preprocess(graph);  // Auto-tunes all parameters

    // Query — exact (optimal)
    auto result = solver.query_exact(0, 2);
    std::cout << "Distance: " << result.distance << "\n";
    std::cout << "Time: " << result.time_microseconds << " us\n";

    // Query — weighted (faster, near-optimal)
    auto fast = solver.query_approx(0, 2, 1.3f);

    // Query — hybrid (fast + optimal)
    auto best = solver.query_hybrid(0, 2);

    return 0;
}
```

### Custom Parameters

```cpp
lops::LOPSParams params;
params.num_potentials = 12;       // Number of LIPS potentials
params.num_anchors = 32;          // Anchors per potential
params.num_landmarks = 8;         // ALT-style landmarks
params.degree_weighted = true;    // Bias anchors toward hubs
params.use_bidirectional = true;  // Enable bidirectional search
params.symmetric_labels = true;   // Symmetric random labels

lops::LOPS solver;
solver.preprocess(graph, params);
```

---

## PHOTON Engine

LOPS is the primary novel algorithm in the **PHOTON** shortest path engine — a C++20 framework providing multiple search strategies:

| Strategy | Description |
|---|---|
| Dijkstra | Classic single-source baseline |
| Bidirectional SIMD | SIMD-accelerated bidirectional Dijkstra |
| Delta-Stepping | Parallel delta-stepping (multi-threaded) |
| A\* | A\* with pluggable heuristic |
| **LOPS/LIPS** | **Lipschitz-Optimized Potential Search** |
| Parallel Wavefront | Lock-free parallel wavefront search |
| Micro-CH | Adaptive micro contraction hierarchy |
| DASH | Degree-Adaptive Shortest-path Heuristic (legacy) |

Engine features:
- Automatic strategy selection based on graph topology
- Thread-safe query cache with LRU eviction
- SIMD-accelerated priority queue (AVX-512, AVX2, ARM NEON)
- OpenMP and TBB parallel support
- Cache-aligned 8-byte edge structures
- Batch query API

---

## Comparison with Other Algorithms

| Algorithm | Preprocessing | Query | Optimality | Coordinates | Best For |
|---|---|---|---|---|---|
| **LOPS** | O(K(V+E) log V) | O((V+E) log V) | 100% | Not needed | General / Non-geometric |
| ALT | O(L(V+E) log V) | O((V+E) log V) | 100% | Not needed | Geometric / Road |
| A\* | O(1) | O((V+E) log V) | 100% | Required | Euclidean |
| Dijkstra | O(1) | O((V+E) log V) | 100% | Not needed | General (baseline) |
| CH | O(V log V · E) | O(log V) | 100% | Not needed | Static road |
| Hub Labeling | O(V · avg_label) | O(label) | 100% | Not needed | Static |

### Key Advantages of LOPS

1. **Complementary to ALT** — Captures structure that landmarks miss in non-geometric graphs
2. **No coordinates needed** — Works on any graph, unlike Euclidean A\*
3. **Provably admissible** — Guarantees optimal paths with exact mode
4. **Auto-tuning** — All parameters self-adjust based on graph topology
5. **Flexible query modes** — Exact, weighted, and hybrid options
6. **Dynamic-friendly** — Preprocessing is lightweight compared to CH

---

## Project Structure

```
LOPS/
├── include/photon/              # Header-only core library
│   ├── lops.hpp                 # LOPS implementation (primary algorithm)
│   ├── photon.hpp               # All-in-one convenience header
│   ├── core.hpp                 # Core types, Edge, PathResult, GraphStats
│   ├── graph.hpp                # Graph data structure (CSR format)
│   ├── engine.hpp               # PhotonEngine, StrategySelector, QueryCache
│   ├── search.hpp               # Dijkstra, Bidirectional, Delta-Stepping, A*
│   ├── simd_queue.hpp           # SIMD-accelerated priority queue
│   └── dash.hpp                 # DASH algorithm (legacy)
│
├── src/                         # Implementation files
│   ├── main.cpp                 # Main executable
│   ├── graph/                   # Graph, builder, Morton cache layout
│   ├── engine/                  # Engine, strategy selector
│   ├── search/                  # All search algorithm implementations
│   ├── queue/                   # SIMD priority queue
│   ├── cache/                   # Query cache
│   └── landmarks/               # Learned landmarks
│
├── research/                    # Research experiments and papers
│   ├── RESEARCH_LOG.md          # Detailed research documentation
│   ├── DASH_paper.tex           # LaTeX paper drafts
│   ├── why_lips_section.md      # LIPS motivation and analysis
│   ├── lips_ablation.cpp        # Budget-matched ALT vs LIPS ablation
│   ├── algorithm_showdown.cpp   # Head-to-head algorithm comparison
│   ├── real_world_benchmark.cpp # SNAP dataset benchmark
│   ├── ewd_*.cpp                # Edge-weighted digraph experiments
│   └── figures/                 # Generated plots (SVG)
│
├── algorithms/                  # Python algorithm prototypes
│   ├── hyperpath.py             # Hyperpath exploration
│   └── novel.py                 # Novel algorithm experiments
│
├── concepts/                    # Concept/prototype code
├── benchmarks/                  # Benchmark harness
├── tests/                       # Unit tests
├── tools/                       # Utilities (SNAP dataset sampler)
├── data/                        # Graph datasets (SNAP)
├── legacy/                      # Legacy DASH research code
├── website/                     # Project website (React/Vite)
├── CMakeLists.txt               # Build system (CMake 3.20+)
└── README.md
```

---

## Research Documentation

See [research/RESEARCH_LOG.md](research/RESEARCH_LOG.md) for complete experimental results and theoretical analysis.

See [research/why_lips_section.md](research/why_lips_section.md) for the motivation behind LIPS as a complementary heuristic to ALT landmarks.

---

## Citation

If you use LOPS in your research, please cite:

```bibtex
@article{lops2026,
  title   = {LOPS: Lipschitz-Optimized Potential Search for
             Shortest Paths in Non-Geometric Graphs},
  author  = {Bhawesh Bhaskar},
  year    = {2026},
  note    = {GitHub: https://github.com/BHAWESHBHASKAR/DASH-Degree-Adaptive-Shortest-path-Heuristic-}
}
```

---

## Roadmap

- [x] Core LOPS/LIPS algorithm
- [x] ALT-style landmark integration
- [x] Combined heuristic (max of LIPS + ALT)
- [x] Bidirectional search support
- [x] Degree-weighted anchor sampling
- [x] Auto-tuning based on graph CV
- [x] Budget-matched ablation study
- [x] SIMD-accelerated priority queue
- [x] SNAP real-world dataset benchmarks
- [ ] Formal paper publication
- [ ] GPU-accelerated preprocessing
- [ ] Dynamic graph support
- [ ] Parallel potential construction

---

## License

MIT License — Free for all use.

---

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

---

## Contact

For questions or collaboration opportunities, please open an issue on GitHub.

---

*Last Updated: February 17, 2026*
