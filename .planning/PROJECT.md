# Isitpossible - Shortest Path Algorithm Research

## What This Is

A research framework for discovering regime boundaries in exact shortest-path algorithms. The goal is to identify when different algorithmic strategies are optimal through systematic experimentation and measurement, not to invent new algorithms without structural justification.

## Core Value

Discover and characterize regimes in which different exact shortest-path strategies are optimal under strict correctness constraints. New understanding matters more than new code.

## Requirements

### Validated

- ✓ Correct algorithm implementations — existing (Dijkstra, A*, bidirectional variants, Contraction Hierarchies)
- ✓ Benchmark infrastructure — existing (workload regime testing, many-to-one, batched queries)
- ✓ Learned components explored — existing (ML-based coordinator, learned CH node ordering)

### Active

- [ ] **Partial Contraction Hierarchies (PCH)** — Novel preprocessing approach that contracts only high-degree nodes (10-20%), reducing preprocessing from O(n²) to O(d·n) while maintaining 5-10x query speedup vs bidirectional search
- [ ] **Regime identification system** — Automated detection of which algorithm should be used based on observable graph characteristics (degree distribution, clustering, planarity)
- [ ] **Comparative benchmarking** — Systematic comparison of PCH vs full CH vs bidirectional across multiple graph types and workload patterns

### Out of Scope

- Inadmissible heuristics or approximate methods — violates exact correctness requirement
- Chasing "beat A*" without structural justification — performance must be explained by regime characteristics
- Algorithm churn or novelty theater — new approaches must change problem structure, not control flow
- Graphs >1500 nodes unless explicitly justified — prevents scaling for the sake of scaling
- Post-hoc parameter tuning — all design decisions must be principled and documented

## Context

**Existing codebase status:**
- Multiple algorithm implementations already available (Dijkstra, A*, CH variants, APEX portfolio selector)
- Extensive benchmarking tools for workload regime testing
- ML components explored (learned coordinator, learned CH ordering)
- Prior work showed APEX2 portfolio selector achieves mixed results (0.78x on dense grids, successful on other graph types)

**Research opportunity:**
Contraction Hierarchies provide excellent query performance (5-100x speedup) but suffer from expensive preprocessing O(n² log n). This limits usefulness for:
- Dynamic graphs (frequent updates require re-preprocessing)
- One-off queries (preprocessing cost not amortized)
- Large graphs (preprocessing time prohibitive)

**Proposed approach:**
Partial Contraction Hierarchies (PCH) — contract only the most important nodes (high-degree, high-centrality) while leaving remaining graph for standard bidirectional search. This creates a continuum between bidirectional search (0% contracted) and full CH (100% contracted).

## Constraints

- **Correctness**: Must be exact — no approximations, all results verified against Dijkstra baseline
- **Graph types**: Focus on specific regimes where PCH has structural advantage (sparse graphs, road networks, grids)
- **Problem structure**: Preprocessing must be cheaper than full CH — target O(d·n) where d << n
- **Metric**: Primary metric is nodes expanded; wall-clock time is secondary
- **Stopping rules**: Abort immediately if correctness fails, preprocessing dominates without amortization, or no structural justification for performance difference

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Focus on preprocessing optimization | CH's main weakness is O(n²) preprocessing, not query performance | — Pending |
| Partial contraction vs novel hierarchy | Extending existing CH approach is more tractable than inventing entirely new structure | — Pending |
| High-degree node selection | Degree is a cheap proxy for node importance in shortest-path queries | — Pending |
| Hybrid query strategy (contracted + bidirectional) | Allows tunable tradeoff between preprocessing cost and query performance | — Pending |

---
*Last updated: 2026-02-04 after initialization*
