# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-02-04)

**Core value:** Discover and characterize regimes in which different exact shortest-path strategies are optimal under strict correctness constraints
**Current focus:** Phase 2 - ML-Based Node Selection

## Current Position

Phase: 2 of 5 (ML-Based Node Selection)
Plan: 3 of 4 in current phase
Status: In progress
Last activity: 2026-02-04 — Completed 02-03-PLAN.md (ML Node Selector Validation)

Progress: [███████░░░] 50%

## Performance Metrics

**Velocity:**
- Total plans completed: 7
- Average duration: 1.6 min
- Total execution time: 0.19 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 01-node-selection-infrastructure | 4 | 4 | 2.0 min |
| 02-ml-based-node-selection | 3 | 4 | 1.3 min |

**Recent Trend:**
- Last 5 plans: 6min, 1min, 1min, 1min, 1min
- Trend: + (insufficient data)

*Updated after each plan completion*

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

**From Phase 1 Plan 1 (01-01):**
- Use total degree (out + in) as primary metric for node importance - most meaningful for contraction hierarchies
- Cache total degree computations only, not individual out/in degrees - balances performance and simplicity
- Manual cache invalidation via clear_cache() - simple, explicit, avoids auto-invalidation complexity
- Dataclass pattern for NodeSelector - leverages automatic __init__, type safety, immutability benefits

**From Phase 1 Plan 2 (01-02):**
- Use sorted() for full ranking - optimal when k ≈ n, simpler than heapq.nlargest(n, ...)
- Use heapq.nlargest() for top-k selection - O(n log k) beats O(n log n) when k << n
- Return Set[int] from selection methods - enables O(1) membership tests for contracted nodes
- Validate inputs aggressively - prevents errors with k=0 or invalid percentages
- Use max(1, ...) for percentage conversion - avoids selecting zero nodes for small percentages

**From Phase 1 Plan 3 (01-03):**
- Use pytest fixtures for graph generation - consistent 100-node count for percentage testing
- Parametrize tests across graph families - ensures generality of node selection algorithms
- Comprehensive edge case coverage - single node and empty graph tests for robustness

**From Phase 1 Plan 4 (01-04):**
- Manual test verification when pytest unavailable - validated all tests work correctly without framework
- Added error handling tests for degree methods to achieve 100% coverage - invalid node IDs are critical edge case
- Stress test with 1000 nodes to verify scalability - ensures selection algorithms work on larger graphs
- Isolated nodes test to verify degree=0 handling - important for sparse graphs in contraction hierarchies
- Correctness tests prove selected nodes are highest-degree - min selected degree >= max non-selected degree

**From Phase 2 Plan 1 (02-01):**
- Use only O(1) features for scalability - avoided expensive centrality measures (betweenness, closeness) that are O(nm) or O(n²)
- Extend NodeSelector via inheritance - maintains clean separation, no modifications to base class
- Graceful fallback to degree when model unavailable - use degree-based selection for backwards compatibility
- Match MLPOrderer pattern from learned_ch_orderer.py - follow existing conventions for consistency
- Feature extraction with 5 O(1) features: degree, out_degree, in_degree, clustering_coefficient, avg_neighbor_degree
- Optional spatial features (spatial_x, spatial_y) when positions provided for geometric graphs

**From Phase 2 Plan 2 (02-02):**
- Use edge difference (original_edges - shortcuts) as contraction benefit label - required to achieve phase goal of improvement over baseline
- Train on 4 graph types (grid, scale_free, geometric, road_like) for diversity - ensures model generalizes across structures
- Sample ~150 nodes per graph to balance quality vs training time - 1800 total samples achieved R² = 0.695
- Follow learned_ch_train.py train_mlp() pattern for consistency - matches existing codebase conventions
- Include y_mean and y_std in model for label denormalization - enables proper inference scaling

**From Phase 2 Plan 3 (02-03):**
- Use edge difference metric for validation - higher values mean fewer shortcuts added during contraction, which is better for CH performance
- Evaluate on multiple graph types (grid, scale-free, geometric) to ensure model generalizes across structures
- Measure Jaccard similarity to verify ML selects different nodes than degree baseline (goal: < 0.8)
- Define improvement as percentage reduction in contraction cost (edge difference)
- ML model achieves 112% average improvement over degree baseline across all test graphs

### Pending Todos

None yet.

### Blockers/Concerns

None yet.

## Session Continuity

Last session: 2026-02-04 22:55 UTC
Stopped at: Completed 02-03-PLAN.md (ML Node Selector Validation)
Resume file: None
