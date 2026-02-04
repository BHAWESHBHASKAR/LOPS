# Technology Stack

**Project:** Isitpossible - Partial Contraction Hierarchies Research
**Researched:** 2026-02-04
**Domain:** Exact shortest-path algorithms (PCH implementation)

## Recommended Stack

### Core Graph Data Structures

| Technology | Version/Type | Purpose | Why |
|------------|-------------|---------|-----|
| **Custom adjacency list (dict-based)** | Built-in Python dict + list | Graph representation | ALREADY USED in codebase. Sparse graph representation with O(1) neighbor access. Memory-efficient for sparse graphs typical of road networks and grids. NetworkX is 40-300x slower; custom implementation essential for research performance. |
| **heapq** | Built-in Python module | Priority queue for Dijkstra/A* | ALREADY USED. Binary heap implementation providing O(log n) push/pop. Standard for shortest-path algorithms. 2025 research confirms binary heaps remain optimal for Dijkstra despite theoretical Fibonacci heap advantages. |
| **set/dict** | Built-in Python | visited tracking, distance arrays | ALREADY USED. O(1) membership testing critical for algorithm correctness. |
| **list/array** | Built-in Python | distance arrays, parent pointers | ALREADY USED. Fast indexed access for distance/parent tracking. |

**Confidence: HIGH** - Based on existing codebase patterns and 2025 benchmarking research showing custom implementations significantly outperform NetworkX (40-300x faster for graph-tool/igraph vs NetworkX).

### Algorithm Implementation Framework

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| **Python dataclasses** | 3.7+ | Graph, PathResult structures | ALREADY USED. Type-safe, minimal boilerplate for data containers. |
| **ABC (Abstract Base Classes)** | Built-in | Algorithm interfaces | ALREADY USED. Enforces consistent API across algorithms (ShortestPathAlgorithm, Heuristic). |
| **typing module** | Built-in | Type hints | ALREADY USED. Essential for research code correctness and maintainability. |

**Confidence: HIGH** - Existing codebase demonstrates this pattern works well.

### Performance Measurement & Benchmarking

| Technology | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **time.perf_counter()** | Built-in | Wall-clock timing | ALREADY USED. Highest-resolution timer, monotonic (never decreases). Use for all algorithm timing. |
| **timeit module** | Built-in | Micro-benchmarking code snippets | Use for benchmarking small, isolated operations (e.g., priority queue operations). NOT for full algorithm runs. |
| **Custom metrics tracking** | Existing (PathResult) | Nodes expanded, edges relaxed | ALREADY USED. Primary research metric is nodes expanded, not wall-clock time. Existing PathResult dataclass captures all needed metrics. |
| **pytest-benchmark** | 5.2.3+ | Fixture-based benchmarking (optional) | Consider for formal benchmarking suite if needed. Provides calibrated rounds and statistical reporting. Not critical for initial PCH work. |

**Why NOT pytest-benchmark initially:** Existing benchmarking infrastructure (workload_regime_benchmark.py, comprehensive_comparison.py) already provides structured measurement. Add pytest-benchmark later if formal regression testing is needed.

**Confidence: HIGH** - time.perf_counter() is established best practice (verified 2025). Custom metrics align with research goals (nodes expanded > wall-clock time).

### Visualization & Results

| Technology | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **matplotlib** | 3.9+ (latest 2025) | Publication-quality plots | Use for final paper figures, performance graphs, regime boundary visualizations. |
| **Custom ASCII visualization** | Existing (visualizations.py) | Quick iteration, terminal output | ALREADY USED. Use during development for fast feedback without plot rendering overhead. |
| **JSON/CSV export** | Built-in json, csv modules | Data persistence | ALREADY USED. Export benchmark results for external analysis and reproducibility. |

**Why NOT Plotly initially:** Interactive plots are unnecessary for research paper figures. Matplotlib produces publication-quality static visualizations. Add Plotly later if interactive exploration is valuable.

**Confidence: HIGH** - Matplotlib remains standard for scientific visualization (verified 2025). Existing ASCII visualization serves development needs.

### Testing & Verification

| Technology | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **pytest** | 8.0+ (latest 2025) | Test framework | Use for unit tests, correctness verification. Rich plugin ecosystem, simple syntax. |
| **Custom correctness assertions** | Existing (PathResult.is_optimal) | Algorithm verification | ALREADY USED. Verify PCH results against Dijkstra baseline with tolerance checks. |
| **Property-based testing** | hypothesis (optional) | Edge case discovery | Consider if PCH has complex invariants. Not needed for initial implementation. |

**Why NOT unittest:** pytest provides superior syntax, fixtures, and ecosystem (verified 2025 research). No reason to use unittest for new research code.

**Confidence: HIGH** - pytest is dominant Python testing framework in 2025. Existing correctness infrastructure works.

### Numerical Computation (if needed)

| Technology | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **math module** | Built-in | Basic math operations | ALREADY USED. Sufficient for distance calculations (hypot, sqrt). |
| **numpy** | 2.0+ | Array operations (optional) | Consider only if doing bulk statistical analysis or vectorized operations. NOT needed for core PCH algorithm. |

**Why avoid numpy initially:** Adds dependency weight. PCH is graph-structural, not numerical. Core algorithm uses dict/list operations, not array math.

**Confidence: MEDIUM** - math module is clearly sufficient. numpy is optional future enhancement if statistical analysis needs grow.

### Graph Generation (Existing)

| Technology | Version | Purpose | Why |
|------------|---------|---------|-----|
| **Custom generators** | Existing (graph_gen.py, maze_generator.py) | Test graph generation | ALREADY USED. Provides grids, road-like planar graphs, mazes. Sufficient for regime testing. |
| **OSM import** | Existing (osm_benchmark.py) | Real-world road networks | ALREADY USED. Optional for validation on real data. |

**Confidence: HIGH** - Existing infrastructure matches research needs.

## Alternatives Considered

### Graph Libraries

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Graph representation | Custom adjacency list | NetworkX 3.6+ | **40-300x slower** than custom/alternatives. High memory overhead (~100 bytes/edge). Unacceptable for performance-critical research. |
| Graph representation | Custom adjacency list | graph-tool | Faster than NetworkX but C++-based, harder to customize for PCH research. Existing codebase is pure Python - maintain consistency. |
| Graph representation | Custom adjacency list | igraph | Similar to graph-tool - faster than NetworkX but less flexible for novel PCH customization. |

**Confidence: HIGH** - 2025 benchmarking research confirms NetworkX performance issues. Custom implementation is correct choice.

### Priority Queue Implementations

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Priority queue | heapq (binary heap) | Fibonacci heap | Theoretical O(1) decrease-key, but poor constant factors. 2025 research confirms binary heaps are faster in practice for Dijkstra. |
| Priority queue | heapq | Sorted list | O(n) insertion, unacceptable. |
| Priority queue | heapq | queue.PriorityQueue | Thread-safe overhead unnecessary for single-threaded algorithms. |

**Confidence: HIGH** - heapq is established standard, verified by 2025 performance research.

### Benchmarking Tools

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Timing | time.perf_counter() | time.time() | Lower resolution, affected by system clock changes. |
| Micro-benchmarks | timeit | timeit.timeit | timeit.repeat() provides better statistics. |
| Benchmark suite | Custom (existing) | pytest-benchmark | Existing infrastructure works. Add pytest-benchmark later if formal regression testing needed. |

**Confidence: HIGH** - time.perf_counter() is authoritative best practice.

### Visualization Libraries

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Static plots | matplotlib | Plotly | Interactive features unnecessary. Matplotlib produces publication-quality figures. |
| Development output | ASCII (existing) | Rich/Textual | Terminal styling adds dependency without research value. Existing ASCII visualization is sufficient. |

**Confidence: MEDIUM** - matplotlib is clearly right for final figures. Development visualization is preference-based.

## Installation

```bash
# Core Python (no external packages needed for PCH algorithm)
# Python 3.11+ recommended for performance improvements

# For visualization (if not already installed)
pip install matplotlib>=3.9

# For testing (if not already installed)
pip install pytest>=8.0

# Optional: benchmarking suite
pip install pytest-benchmark>=5.2

# Optional: statistical analysis
pip install numpy>=2.0
```

**Note:** The existing codebase appears to use only standard library (heapq, time, math, json, dataclasses, typing, abc). Verify no hidden dependencies by checking imports across all algorithm files.

## Architecture Notes

### Data Structure Choices

**Graph Representation:**
```python
# Adjacency list (existing pattern)
edges: Dict[int, List[Tuple[int, float]]]
# node -> [(neighbor, weight), ...]
```

**Why this works:**
- Sparse graphs (road networks, grids) have O(n) edges, not O(n²)
- O(1) neighbor iteration for Dijkstra relaxation
- Memory-efficient: stores only existing edges
- Easy to add shortcuts (PCH requires this)

**Reverse Graph for CH:**
```python
# Build reverse graph on demand (existing pattern in contraction_hierarchy.py)
reverse = {u: [] for u in graph}
for u, edges in graph.items():
    for v, w in edges:
        reverse[v].append((u, w))
```

**Why:** Bidirectional search and CH query require reverse edges. Build once, store as dict.

### Priority Queue Usage

**Standard Dijkstra pattern (existing):**
```python
pq = [(0, source)]  # (distance, node)
dist = [inf] * n
while pq:
    d, u = heapq.heappop(pq)
    if d > dist[u]: continue  # Stale entry check
    for v, w in graph.edges[u]:
        if d + w < dist[v]:
            dist[v] = d + w
            heapq.heappush(pq, (dist[v], v))
```

**PCH-specific optimization:**
- Node ordering requires priority queue of (priority, node) for contraction order
- Witness search uses bounded Dijkstra with priority queue
- Query uses bidirectional search with two priority queues

### Performance Considerations

**Memory for n ≤ 1500 nodes:**
- Graph edges: O(n) adjacency lists ~ 10-50KB
- Distance arrays: O(n) floats ~ 12KB
- Priority queue: O(n) entries ~ 20-50KB
- **Total:** < 1MB per graph instance. Memory is not a constraint.

**Time complexity targets:**
- Full CH preprocessing: O(n² log n) baseline
- PCH preprocessing: O(d·n log n) where d = contracted nodes (10-20% of n)
- Expected PCH speedup: 5-10x reduction in preprocessing time
- Query performance: Maintain 5-10x vs bidirectional search

## Sources

### Graph Data Structures & Performance

- **HIGH Confidence:** StackOverflow discussion on efficient Python graph structures (ongoing community consensus)
- **HIGH Confidence:** GeeksforGeeks adjacency list vs matrix comparison (July 2025)
- **MEDIUM Confidence:** 2025 IJCA research paper on DFS performance benchmarks (Python vs Java)
- **MEDIUM Confidence:** freeCodeCamp graph algorithms guide (September 2025)

### Priority Queues

- **HIGH Confidence:** 2025 comparative analysis of priority queue variants in Dijkstra (RSIS International Journal, September 2025)
- **HIGH Confidence:** GeeksforGeeks heap queue guide (updated May 2025)
- **HIGH Confidence:** Stanford implementation notes for Dijkstra optimization (November 2025)

### Benchmarking & Performance

- **HIGH Confidence:** 2025 Python benchmarking research (MDPI, various publications)
- **HIGH Confidence:** pytest-benchmark 5.2.3 documentation and best practices
- **HIGH Confidence:** time.perf_counter() as authoritative timer (Python docs, verified 2025)

### Contraction Hierarchies

- **MEDIUM Confidence:** GitHub LdDl/ch Python implementation (community resource)
- **MEDIUM Confidence:** Medium tutorial on CH routing (2025)
- **LOW Confidence:** 2025 research papers on parallel CH (requires verification of applicability to PCH)

### Testing Frameworks

- **HIGH Confidence:** pytest as dominant Python testing framework (verified across multiple 2025 sources)
- **HIGH Confidence:** pytest-benchmark for micro-benchmarking (5.2.3 current version)

### NetworkX Performance

- **HIGH Confidence:** 2025 comparative evaluation showing graph-tool 40-250x faster than NetworkX
- **HIGH Confidence:** NetworkX memory and performance limitations documented in recent research

### Visualization

- **HIGH Confidence:** Matplotlib and Plotly as top Python visualization libraries in 2025 (Reflex, January 2025)
- **MEDIUM Confidence:** Matplotlib vs Plotly comparison (Scribd, October 2025)

### Existing Codebase Analysis

- **HIGH Confidence:** Direct inspection of isitpossible codebase (algorithms/baseline.py, contraction_hierarchy.py, workload_regime_benchmark.py, visualizations.py, base.py)
- **HIGH Confidence:** Verified existing patterns: custom Graph class, heapq usage, PathResult metrics, ASCII visualization
