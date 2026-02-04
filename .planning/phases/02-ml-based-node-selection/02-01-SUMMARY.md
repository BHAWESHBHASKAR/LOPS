---
phase: 02-ml-based-node-selection
plan: 01
subsystem: ml-selection
tags: [mlp, feature-extraction, node-selection, contraction-hierarchies, degree-fallback]

# Dependency graph
requires:
  - phase: 01-node-selection-infrastructure
    provides: NodeSelector base class with degree-based selection
provides:
  - MLNodeSelector class extending NodeSelector with ML-predicted scores
  - Feature extraction utilities for graph nodes with O(1) complexity
  - Degree-based fallback when ML model unavailable
affects: [02-02, 02-03, 02-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Inheritance-based extension: MLNodeSelector extends NodeSelector"
    - "Graceful degradation: ML model unavailable falls back to degree"
    - "O(1) feature extraction: degree, clustering, neighbor features only"

key-files:
  created:
    - algorithms/ml_features.py
    - algorithms/ml_node_selector.py
  modified: []

key-decisions:
  - "Use only O(1) features for scalability - avoid expensive centrality measures"
  - "Extend NodeSelector via inheritance rather than modification"
  - "Graceful fallback to degree when model unavailable - no breaking changes"
  - "Match existing MLPOrderer pattern from learned_ch_orderer.py"

patterns-established:
  - "ML scoring with degree fallback pattern"
  - "Feature extraction with optional spatial features"
  - "Model loading with error handling and warnings"

# Metrics
duration: 1min
completed: 2026-02-04
---

# Phase 2 Plan 1: ML-Based Node Selection Infrastructure Summary

**MLNodeSelector class with ML-predicted node importance scores, O(1) feature extraction, and graceful degree-based fallback**

## Performance

- **Duration:** 1 min
- **Started:** 2026-02-04T22:50:30Z
- **Completed:** 2026-02-04T22:51:24Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments

- Created feature extraction utilities with 5 O(1) features (degree, clustering, neighbors)
- Implemented MLNodeSelector extending NodeSelector with ML-predicted scoring
- Added graceful degree-based fallback when model unavailable
- Matched existing MLPOrderer pattern for consistency

## Task Commits

Each task was committed atomically:

1. **Task 1: Create feature extraction utilities** - `6b8d0f6` (feat)
2. **Task 2: Create MLNodeSelector class** - `419c7fe` (feat)

**Plan metadata:** [pending final commit]

## Files Created/Modified

- `algorithms/ml_features.py` - Feature extraction with extract_node_features() function
  - O(1) features: degree, out_degree, in_degree, clustering_coefficient, avg_neighbor_degree
  - Optional spatial features (spatial_x, spatial_y) from positions
  - Triangle counting for clustering coefficient computation
- `algorithms/ml_node_selector.py` - MLNodeSelector class extending NodeSelector
  - ml_score() method with ML-predicted importance scores
  - Degree-based fallback when model unavailable
  - select_top_k() and select_top_k_percent() overrides using ML scores
  - Graceful error handling for missing/invalid model files

## Decisions Made

- **Use only O(1) features for scalability** - Avoided expensive centrality measures (betweenness, closeness) that are O(nm) or O(n²)
- **Extend NodeSelector via inheritance** - Maintains clean separation, no modifications to base class
- **Graceful fallback to degree** - When model unavailable, use degree-based selection for backwards compatibility
- **Match MLPOrderer pattern** - Follow existing conventions from learned_ch_orderer.py for consistency

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - all tasks completed without issues.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

MLNodeSelector infrastructure complete and ready for:
- Training ML models on graph features (Phase 2 Plan 2)
- Integration with contraction hierarchy construction (Phase 2 Plan 3)
- Performance evaluation against degree-based selection (Phase 2 Plan 4)

No blockers or concerns.

---
*Phase: 02-ml-based-node-selection*
*Plan: 01*
*Completed: 2026-02-04*
