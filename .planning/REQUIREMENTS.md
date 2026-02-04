# Requirements: Isitpossible - Partial Contraction Hierarchies

**Defined:** 2026-02-04
**Core Value:** Discover regime boundaries in exact shortest-path algorithms through Partial Contraction Hierarchies

## v1 Requirements

### Node Selection

- [ ] **NS-01**: System can compute node degree for all nodes in graph
- [ ] **NS-02**: System can rank nodes by degree in descending order
- [ ] **NS-03**: System can select top k% of nodes for contraction where k is configurable (5%, 10%, 15%, 20%)
- [ ] **NS-04**: System uses ML-based model to score node importance (extends/learned_ch_orderer.py)
- [ ] **NS-05**: System can train ML model on graph features to predict contraction benefit

### Partial Contraction

- [ ] **PC-01**: System can contract selected nodes using witness search from existing CH implementation
- [ ] **PC-02**: System inserts shortcuts for witness paths with bounds checking (max shortcuts = 3-5x original edges)
- [ ] **PC-03**: System tracks preprocessing metrics: nodes contracted, shortcuts added, preprocessing time
- [ ] **PC-04**: System can export preprocessed graph structure (contracted nodes, shortcuts) for reuse

### Hybrid Query Strategy

- [ ] **HQ-01**: System executes two-phase query: CH-up search on contracted nodes, bidirectional search on uncontracted remainder
- [ ] **HQ-02**: System can stitch paths from both phases into single optimal path
- [ ] **HQ-03**: System implements LRU cache for query results with configurable cache size
- [ ] **HQ-04**: System can retrieve cached results in O(1) time for repeated queries

### Correctness & Validation

- [ ] **CV-01**: System verifies all query results match Dijkstra baseline distance exactly
- [ ] **CV-02**: System collects query metrics: nodes expanded (forward + backward), query time (ms), path validity
- [ ] **CV-03**: System detects and reports correctness violations with details (query pair, expected vs actual)
- [ ] **CV-04**: System enforces shortcut limits (abort if shortcuts > 5x original edges)

### Benchmarking & Research

- [ ] **BR-01**: System can run contraction sweep testing k ∈ {5%, 10%, 15%, 20%} on same graph
- [ ] **BR-02**: System can test on multiple graph types: grids, road networks, random geometric graphs
- [ ] **BR-03**: System compares PCH against baselines: full CH, bidirectional Dijkstra, A*
- [ ] **BR-04**: System exports results to JSON with all metrics for analysis
- [ ] **BR-05**: System identifies regime boundaries where PCH outperforms baselines

## v2 Requirements

### Advanced Node Selection

- **NS-V2-01**: System supports multiple importance metrics (edge-betweenness centrality, PageRank, closeness centrality)
- **NS-V2-02**: System can compare different node selection strategies side-by-side
- **NS-V2-03**: System adapts contraction percentage based on graph characteristics automatically

### Performance Optimization

- **PO-V2-01**: System optimizes path unpacking bottleneck (separate unpacking time metric)
- **PO-V2-02**: System implements incremental preprocessing (update contracted graph without full rebuild)
- **PO-V2-03**: System pre-computes query metadata for faster hybrid search

### Production Features

- **PF-V2-01**: System provides REST API for query execution
- **PF-V2-02**: System supports batch query processing with parallelization
- **PF-V2-03**: System implements graph serialization/deserialization (save/load preprocessed graphs)

## Out of Scope

| Feature | Reason |
|---------|--------|
| Approximate algorithms | Violates exact correctness requirement |
| Random node contraction | No theoretical basis for correctness or performance |
| Unbounded caching | Memory explosion risk; cache size must be bounded |
| Graphs > 1500 nodes | Scope constraint; prevents scaling for scaling's sake |
| Post-hoc parameter tuning | All design decisions must be principled and pre-specified |
| Adaptive contraction without correctness proofs | Violates core value of exact correctness |
| Multi-criteria shortest paths | Out of scope for single-source single-target regime research |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| NS-01 | Phase 1 | Complete |
| NS-02 | Phase 1 | Complete |
| NS-03 | Phase 1 | Complete |
| NS-04 | Phase 2 | Complete |
| NS-05 | Phase 2 | Complete |
| PC-01 | Phase 3 | Pending |
| PC-02 | Phase 3 | Pending |
| PC-03 | Phase 3 | Pending |
| PC-04 | Phase 3 | Pending |
| CV-04 | Phase 3 | Pending |
| HQ-01 | Phase 4 | Pending |
| HQ-02 | Phase 4 | Pending |
| HQ-03 | Phase 4 | Pending |
| HQ-04 | Phase 4 | Pending |
| CV-01 | Phase 4 | Pending |
| CV-02 | Phase 4 | Pending |
| CV-03 | Phase 4 | Pending |
| BR-01 | Phase 5 | Pending |
| BR-02 | Phase 5 | Pending |
| BR-03 | Phase 5 | Pending |
| BR-04 | Phase 5 | Pending |
| BR-05 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 21 total
- Mapped to phases: 21
- Unmapped: 0 ✓

---
*Requirements defined: 2026-02-04*
*Last updated: 2026-02-04 after roadmap creation*
