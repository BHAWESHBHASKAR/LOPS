# Roadmap: Isitpossible - Partial Contraction Hierarchies

## Overview

This roadmap develops a novel Partial Contraction Hierarchies (PCH) algorithm that occupies the continuum between bidirectional search and full Contraction Hierarchies. We build node selection infrastructure, implement partial contraction with ML-based importance scoring, create a hybrid query system that switches between overlay and bidirectional search, and validate through systematic benchmarking to discover regime boundaries where PCH outperforms baselines.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Node Selection Infrastructure** - Degree-based node importance scoring
- [ ] **Phase 2: ML-Based Node Selection** - Learned node contraction ordering
- [ ] **Phase 3: Partial Contraction** - Selective node contraction with shortcuts
- [ ] **Phase 4: Hybrid Query System** - Overlay + bidirectional search integration
- [ ] **Phase 5: Benchmarking & Regime Analysis** - Empirical validation and regime discovery

## Phase Details

### Phase 1: Node Selection Infrastructure

**Goal**: System can identify and select high-importance nodes for contraction using degree-based metrics

**Depends on**: Nothing (first phase)

**Requirements**: NS-01, NS-02, NS-03

**Success Criteria** (what must be TRUE):
1. User can compute node degree for all nodes in any graph
2. User can rank nodes by degree and see top-k nodes
3. User can configure contraction percentage (k) and get selected node set
4. System validates selected nodes are properly ranked by degree

**Plans**: 4 plans

Plans:
- [x] 01-01-PLAN.md — Create NodeSelector class with degree computation and caching
- [x] 01-02-PLAN.md — Implement node ranking and top-k selection methods
- [x] 01-03-PLAN.md — Add comprehensive parametrized test suite
- [x] 01-04-PLAN.md — Comprehensive selection tests and coverage verification

### Phase 2: ML-Based Node Selection

**Goal**: System uses ML models to predict which nodes provide highest contraction benefit

**Depends on**: Phase 1

**Requirements**: NS-04, NS-05

**Success Criteria** (what must be TRUE):
1. User can train ML model on graph features to predict node importance
2. User can use trained model to score nodes for contraction
3. System extends existing learned_ch_orderer.py for PCH context
4. Model predictions improve node selection vs degree-only baseline

**Plans**: 3 plans

Plans:
- [ ] 02-01-PLAN.md — Create MLNodeSelector class and feature extraction utilities
- [ ] 02-02-PLAN.md — Implement training pipeline for graph feature-based node scoring
- [ ] 02-03-PLAN.md — Validate ML model performance against degree-based baseline

### Phase 3: Partial Contraction

**Goal**: System contracts selected subset of nodes and creates overlay graph with shortcuts

**Depends on**: Phase 1, Phase 2

**Requirements**: PC-01, PC-02, PC-03, PC-04, CV-04

**Success Criteria** (what must be TRUE):
1. User can contract selected nodes using witness search from existing CH
2. System inserts shortcuts with bounds checking (max 3-5x original edges)
3. User can export and reload preprocessed graph structure
4. System tracks and reports preprocessing metrics (nodes contracted, shortcuts added, time)
5. System enforces shortcut limits and aborts if exceeded

**Plans**: 3 plans

Plans:
- [ ] 03-01: Adapt existing CH contraction loop for partial node processing
- [ ] 03-02: Implement shortcut insertion with bounds checking
- [ ] 03-03: Add preprocessing metrics tracking and graph export/import

### Phase 4: Hybrid Query System

**Goal**: System executes two-phase queries combining overlay CH search and bidirectional search

**Depends on**: Phase 3

**Requirements**: HQ-01, HQ-02, HQ-03, HQ-04, CV-01, CV-02, CV-03

**Success Criteria** (what must be TRUE):
1. User can execute queries that use CH-up search on contracted nodes and bidirectional on remainder
2. System can stitch paths from both phases into single optimal path
3. User can retrieve cached query results in O(1) time
4. System verifies all query results match Dijkstra baseline exactly
5. System detects and reports correctness violations with diagnostic details
6. System collects query metrics (nodes expanded, query time, path validity)

**Plans**: 4 plans

Plans:
- [ ] 04-01: Implement two-phase query execution (overlay + bidirectional)
- [ ] 04-02: Implement path stitching logic for hybrid results
- [ ] 04-03: Add LRU cache for query results
- [ ] 04-04: Implement correctness verification and metrics collection

### Phase 5: Benchmarking & Regime Analysis

**Goal**: System empirically validates PCH performance and identifies regime boundaries

**Depends on**: Phase 4

**Requirements**: BR-01, BR-02, BR-03, BR-04, BR-05

**Success Criteria** (what must be TRUE):
1. User can run contraction sweep testing k in {5%, 10%, 15%, 20%} on same graph
2. User can benchmark PCH on multiple graph types (grids, road networks, random geometric)
3. System compares PCH against baselines (full CH, bidirectional Dijkstra, A*)
4. System exports all results to JSON for analysis
5. System identifies and reports regime boundaries where PCH outperforms baselines

**Plans**: 3 plans

Plans:
- [ ] 05-01: Implement contraction sweep testing framework
- [ ] 05-02: Add multi-graph-type benchmarking infrastructure
- [ ] 05-03: Implement baseline comparison and regime boundary detection

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Node Selection Infrastructure | 4/4 | Complete ✓ | 2026-02-04 |
| 2. ML-Based Node Selection | 3/3 | Complete ✓ | 2026-02-04 |
| 3. Partial Contraction | 0/3 | Not started | - |
| 4. Hybrid Query System | 0/4 | Not started | - |
| 5. Benchmarking & Regime Analysis | 0/3 | Not started | - |
