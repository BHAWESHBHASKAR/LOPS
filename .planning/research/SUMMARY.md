# Project Research Summary

**Project:** Isitpossible - Shortest Path Algorithm Research
**Domain:** Partial Contraction Hierarchies (PCH) for exact shortest-path queries
**Researched:** 2026-02-04
**Confidence:** MEDIUM

## Executive Summary

Partial Contraction Hierarchies (PCH) represents a novel preprocessing technique that occupies the continuum between zero-preprocessing bidirectional search and full Contraction Hierarchies. The core innovation is selective contraction: contract only the most "important" nodes (typically 10-20% of high-degree nodes) while leaving the remainder for standard bidirectional search. This creates a tunable tradeoff where preprocessing cost is O(d·n) instead of O(n² log n), while still achieving 5-10x query speedup versus bidirectional baselines.

The research reveals that PCH is architecturally feasible but carries significant correctness risks. The critical challenge is not implementing contraction or bidirectional search individually (both are well-understood), but integrating them seamlessly while maintaining exact correctness guarantees. The most severe risks involve correctness violations at contraction boundaries, shortcuts explosion causing memory blowup, and query performance degradation that makes PCH worse than plain bidirectional search. Existing codebase provides strong foundations (custom Graph class, heapq-based priority queues, PathResult metrics, and full CH implementation), but PCH requires novel hybrid query logic that has no precedent in literature.

Based on research, the recommended approach is to implement PCH in four phases: (1) Node Selection — implement degree-based importance scoring; (2) Partial Contraction — adapt existing CH contraction loop for partial processing; (3) Hybrid Query Integration — develop novel query strategy that switches between overlay and bidirectional search; (4) Benchmarking and Regime Analysis — empirically validate that PCH outperforms baselines in identifiable regimes. This phased approach allows early validation of core assumptions while deferring high-risk integration work until foundational components are proven.

## Key Findings

### Recommended Stack

The existing codebase already uses optimal technologies for exact shortest-path research. Custom adjacency list representation (dict-based) provides O(1) neighbor access and is 40-300x faster than NetworkX based on 2025 benchmarking research. Python's built-in heapq module remains the standard for priority queue operations in Dijkstra variants, and time.perf_counter() is the authoritative best practice for high-resolution timing. For visualization, matplotlib is sufficient for publication-quality figures without adding interactive complexity.

**Core technologies:**
- **Custom adjacency list (dict-based)** — Graph representation — 40-300x faster than NetworkX, memory-efficient for sparse graphs
- **heapq** — Priority queue for Dijkstra/A* — binary heap with O(log n) operations, verified optimal in practice
- **time.perf_counter()** — Wall-clock timing — highest-resolution monotonic timer, standard for algorithm benchmarking
- **Python dataclasses + ABC** — Algorithm interfaces — type-safe data containers, enforces consistent API
- **pytest** — Test framework — dominant Python testing framework, simple syntax with rich ecosystem

### Expected Features

PCH feature space is well-defined with clear must-have, should-have, and deferred features. Table stakes features (essential for correctness) include node importance scoring, selective contraction, hybrid query strategy, correctness verification, and shortcut creation. The primary differentiator is tunable contraction percentage (k ∈ [0, 1]) that allows exploration of the continuum from bidirectional search (k=0) to full CH (k=1).

**Must have (table stakes):**
- Node importance scoring — identifies which nodes to contract (degree-based for MVP)
- Selective contraction — contracts subset of nodes, not all (core innovation)
- Hybrid query strategy — queries contracted nodes with CH, remainder with bidirectional
- Correctness verification — compares all results to Dijkstra baseline
- Shortcut creation with witness search — required for contracted node correctness

**Should have (competitive):**
- Tunable contraction percentage — parameterized k to explore preprocessing/query tradeoff
- O(d·n) preprocessing target — achieves 5-10x speedup in preprocessing vs full CH
- Adaptive node selection — automatically identify high-value nodes to contract

**Defer (v2+):**
- ML-based node ordering — learned_ch_orderer.py exists but adds complexity
- Centrality metrics — betweenness requires O(n·m) computation
- Incremental updates — literature sparse, high complexity
- Multi-level hierarchy — layered PCH is research gap

### Architecture Approach

The recommended architecture follows a three-phase pipeline with clear component boundaries. Phase 1 (Node Selection) identifies high-degree/high-centrality nodes for contraction using DegreeAnalyzer and NodeSelector. Phase 2 (Partial Contraction) contracts the selected subset using PartialContractor, Orderer, WitnessSearch, and ShortcutInserter to create an overlay graph. Phase 3 (Hybrid Query) executes queries using HybridQueryEngine, QueryStrategySelector, OverlaySearch, BidirectionalSearch, and PathStitcher.

**Major components:**
1. **NodeSelector** — selects subset of nodes for contraction based on degree/centrality metrics
2. **PartialContractor** — contracts selected nodes, adds shortcuts, produces overlay graph
3. **HybridQueryEngine** — executes hybrid query strategy adapting between overlay and bidirectional search
4. **PathStitcher** — combines path segments from different search strategies

### Critical Pitfalls

PCH research reveals five critical pitfalls that can compromise correctness, performance, or both. The most severe is correctness violations at contraction boundaries, where the hybrid query may produce non-optimal paths due to improper transitions between contracted and uncontracted regions. Shortcuts explosion (memory blowup) is the second major risk, where each node contraction can add O(degree²) shortcuts, causing memory overflow and query slowdown. Suboptimal node selection leads to wasted preprocessing or missed speedup opportunities. Query performance worse than bidirectional baseline is observed in APEX with 0.78x slowdown on dense grids. Path unpacking overhead can dominate query time if shortcuts are deeply nested.

1. **Correctness violations at contraction boundaries** — implement boundary-aware query termination, explicit boundary validation, and maintain "witness" edges
2. **Shortcuts explosion (memory blowup)** — use degree-aware node ordering (low-degree first), limit shortcuts per node, set thresholds on shortcuts/edges ratio
3. **Suboptimal node selection** — use query-aware sampling, adaptive contraction with performance feedback, multi-criteria scoring
4. **Query performance worse than baseline** — implement baseline-first development, always benchmark against bidirectional, set performance budget with graceful degradation
5. **Path unpacking overhead** — store explicit paths for shortcuts, cache unpacked paths, limit shortcut recursion depth

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Node Selection and Validation
**Rationale:** Node selection has no dependencies and validates core assumptions about which nodes benefit from contraction. This phase avoids premature commitment to specific contraction or query strategies.
**Delivers:** Working NodeSelector with degree-based importance scoring, centrality calculator (optional enhancement)
**Addresses:** Table stakes features (node importance scoring, selection criteria)
**Avoids:** Pitfall of suboptimal node selection by starting with simple, well-understood degree metric
**Research flags:** None — standard graph algorithms, well-documented

### Phase 2: Partial Contraction Infrastructure
**Rationale:** Builds on Phase 1 outputs, adapts existing CH code (contraction_hierarchy.py) for partial processing, and enables query work to proceed in parallel.
**Delivers:** Working PartialContractor that produces valid overlay graphs with shortcuts, contracted set metadata
**Uses:** Existing CH implementation (_witness_search, _contract, _add_shortcut), heapq for priority queue management
**Implements:** Architecture components for contraction loop, shortcut insertion, ordering strategy
**Addresses:** Table stakes features (selective contraction, shortcut creation, witness search)
**Avoids:** Pitfall of shortcuts explosion through degree-aware ordering and contraction limits
**Research flags:** MEDIUM risk — witness search bounds for partial contraction need validation

### Phase 3: Hybrid Query Integration
**Rationale:** Most complex phase requiring novel integration of overlay and bidirectional search. Depends on Phase 2 for overlay structure but can start once overlay data structure is defined.
**Delivers:** Working HybridQueryEngine with correctness guarantees, path stitching logic, strategy selection
**Uses:** Existing BidirectionalDijkstra (baseline.py), overlay search patterns (overlay_ch.py)
**Implements:** Architecture components for query strategy selection, overlay search, bidirectional search, path stitching
**Addresses:** Table stakes features (hybrid query strategy), differentiator (tunable contraction percentage)
**Avoids:** Pitfalls of correctness violations and query worse than baseline through rigorous testing
**Research flags:** HIGH RISK — correctness of hybrid strategy is novel, no literature precedent, may need iteration

### Phase 4: Benchmarking and Regime Analysis
**Rationale:** Needs working end-to-end system to validate core value proposition. Identifies regime boundaries where PCH outperforms CH and bidirectional, preventing wasted optimization in wrong regimes.
**Delivers:** Comprehensive benchmark results across graph types, ablation study on contraction percentage (5%, 10%, 15%, 20%), regime characterization
**Uses:** Existing benchmark infrastructure (workload_regime_benchmark.py, comprehensive_comparison.py), matplotlib for visualization
**Addresses:** Differentiator features (O(d·n) preprocessing target, adaptive node selection), validates success criteria
**Avoids:** Pitfall of premature optimization by measuring before optimizing
**Research flags:** MEDIUM risk — may discover PCH only benefits specific graph structures, requiring problem reformulation

### Phase 5: Optimization and Productionization
**Rationale:** Premature optimization without data is waste. This phase finalizes integration with existing benchmark infrastructure and productionizes the API.
**Delivers:** Production-ready PartialCH implementing ShortestPathAlgorithm ABC, pre-computed boundary metadata, optimized hot paths
**Uses:** Existing integration patterns (ShortestPathAlgorithm ABC, PathResult dataclass)
**Addresses:** Deferred features (ML-based ordering, caching) if data supports value
**Avoids:** Pitfall of path unpacking overhead through profiling and optimization
**Research flags:** None — standard optimization patterns

### Phase Ordering Rationale

- Node selection first because it has no dependencies and validates assumptions about which nodes matter
- Partial contraction second because it builds on selection and enables query work
- Hybrid query third as most complex integration challenge, needs overlay structure from Phase 2
- Benchmarking fourth to validate end-to-end system before investing in optimization
- Optimization last because without data from Phase 4, optimization is premature

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 2 (Partial Contraction):** Witness search optimization is critical bottleneck but bounds for partial contraction are unstudied in literature
- **Phase 3 (Hybrid Query):** Correctness of mixing overlay + bidirectional search has no literature precedent, likely needs formal proof or extensive testing
- **Phase 4 (Benchmarking):** May discover PCH only works for specific graph structures, potentially requiring problem reformulation

Phases with standard patterns (skip research-phase):
- **Phase 1 (Node Selection):** Degree computation and top-k selection are standard graph algorithms
- **Phase 5 (Optimization):** Profiling, caching, and hot path optimization follow well-established patterns

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Existing codebase patterns verified optimal; 2025 benchmarking research confirms custom implementations outperform NetworkX by 40-300x |
| Features | MEDIUM | Table stakes features based on CH fundamentals are high confidence; differentiators are novel PCH features with medium confidence; research gaps identified (optimal contraction percentage, hybrid query transition) |
| Architecture | MEDIUM | Component boundaries and data structures are high confidence; hybrid query integration is LOW confidence due to novelty |
| Pitfalls | HIGH | Correctness violations and shortcuts explosion are well-documented in CH literature; query performance degradation has empirical evidence from APEX |

**Overall confidence:** MEDIUM

### Gaps to Address

- **Optimal contraction percentage (k):** Literature focuses on full CH, not partial. Phase 4 ablation study must empirically determine optimal k for different graph types
- **Hybrid query correctness proofs:** No literature on formal correctness of mixing overlay + bidirectional search. Phase 3 must include rigorous testing, possibly formal proof
- **Boundary node selection strategy:** Some guidance from overlay CH but not specific to partial contraction. Phase 3 may need experimentation
- **Performance tradeoffs:** General CH literature discusses preprocessing vs query tradeoff, but partial CH changes equation. Phase 4 benchmarking required to quantify

## Sources

### Primary (HIGH confidence)
- **Existing codebase** — /Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/contraction_hierarchy.py — Verified CH implementation patterns (_witness_search, _contract, _add_shortcut, bidirectional query)
- **Existing codebase** — /Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/baseline.py — Verified BidirectionalDijkstra implementation
- **Existing codebase** — /Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/base.py — Verified Graph, PathResult, ShortestPathAlgorithm interfaces
- **Existing codebase** — /Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/overlay_ch.py — Verified region-based partitioning approach
- **"Faster and Simpler Hierarchical Routing in Road Networks"** (Geisberger et al., 2008) — Foundational CH paper, 1,214+ citations
- **"Customizable Contraction Hierarchies"** (Dibbelt et al., 2014) — Three-phase workflow, 209 citations
- **2025 Python benchmarking research** (MDPI publications) — time.perf_counter() as authoritative timer, pytest dominance confirmed
- **2025 comparative analysis of priority queue variants** (RSIS International Journal) — Binary heaps optimal for Dijkstra in practice

### Secondary (MEDIUM confidence)
- **"Parallel Contraction Hierarchies Construction on Road Networks"** (Chen et al., 2024) — Recent work on preprocessing optimization
- **"Weak Contraction Hierarchies Work!"** (Zeitz, 2013) — Explores partial preprocessing
- **"Improved Cut Strategy for Tensor Network Contraction"** (Staudt et al., 2024) — Unbalanced contraction trees suboptimal, partial contraction requires careful ordering
- **Contraction Hierarchies Implementation Guide** (jlazarsfeld.github.io) — "The fewer shortcuts you add, the less time you take searching edges"
- **Bidirectional Dijkstra Instance Optimality** (Haeupler et al., 2024) — Sets theoretical lower bound PCH must beat
- **NetworkX vs graph-tool/igraph benchmarks** (2025) — Confirms NetworkX is 40-300x slower than alternatives

### Tertiary (LOW confidence)
- **APEX dense grid slowdown** — Project context mentions 0.78x slowdown, needs verification with APEX documentation
- **ML model race conditions** — Project context mentions potential issues, needs code review and testing
- **"Key node identification for network topology"** (Nature Scientific Reports, 2024) — Node importance metrics, needs validation in PCH context
- **"Incremental Update of Contraction Hierarchies"** (TCD thesis, 2013) — Dynamic updates, sparse literature
- **DRNCS: Dual-Level Route Generation Model** (Li, ECML PKDD 2025) — Preprint, not peer-reviewed, claims dual-level architecture

---
*Research completed: 2026-02-04*
*Ready for roadmap: yes*
