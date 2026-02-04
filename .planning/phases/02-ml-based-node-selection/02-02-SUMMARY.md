---
phase: 02-ml-based-node-selection
plan: 02
subsystem: ml-training
tags: [numpy, mlp, edge-difference, contraction-benefit, graph-features]

# Dependency graph
requires:
  - phase: 02-01
    provides: MLNodeSelector class, ml_features FEATURE_KEYS, MLPOrderer pattern
provides:
  - Training pipeline for ML node selector with edge difference labels
  - Trained MLP model (R² = 0.695) predicting contraction benefit
  - Model serialization format matching MLPOrderer.load() pattern
affects: [02-03, 02-04]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Edge difference metric for contraction benefit (original_edges - shortcuts)
    - MLP training with SGD and gradient clipping
    - Multi-graph-type training data generation
    - Model JSON serialization with normalization parameters

key-files:
  created:
    - tools/train_ml_node_selector.py - Training pipeline with contraction simulation
    - models/ml_node_selector.json - Trained MLP model weights and parameters
  modified: []

key-decisions:
  - Use edge difference (NOT degree proxy) as label to enable learning actual contraction patterns
  - Train on 4 graph types (grid, scale_free, geometric, road_like) for diversity
  - Sample ~150 nodes per graph to balance quality vs training time
  - Follow learned_ch_train.py train_mlp() pattern for consistency
  - Include y_mean and y_std in model for label denormalization

patterns-established:
  - "Contraction simulation: Compute edge difference by simulating shortcut addition"
  - "Training diversity: Generate samples from multiple graph families"
  - "Model persistence: Save weights + normalization params for inference"

# Metrics
duration: 1min
completed: 2026-02-04
---

# Phase 2 Plan 2: ML Node Selector Training Pipeline Summary

**Training pipeline with edge difference labels achieving R² = 0.695 on contraction benefit prediction using 5 O(1) graph features**

## Performance

- **Duration:** 1 min (51s)
- **Started:** 2026-02-04T22:52:14Z
- **Completed:** 2026-02-04T22:53:05Z
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments

- Created training pipeline that simulates actual node contraction (not degree proxy)
- Generated 1800 training samples from 4 diverse graph types
- Trained MLP model with 16 hidden units using SGD with gradient clipping
- Achieved R² = 0.695 on predicting edge difference (contraction benefit)
- Verified MLNodeSelector can load and use trained model for inference

## Task Commits

Each task was committed atomically:

1. **Task 1: Create training pipeline script** - `7d449d6` (feat)

**Plan metadata:** [pending final commit]

_Note: TDD tasks may have multiple commits (test → feat → refactor)_

## Files Created/Modified

- `tools/train_ml_node_selector.py` - Training pipeline with edge difference simulation
  - train_mlp(): MLP training with SGD, gradient clipping, ReLU activation
  - simulate_contraction_benefit(): Computes edge difference = original_edges - shortcuts
  - generate_training_data(): Samples nodes from grid, scale_free, geometric, road_like graphs
  - Follows learned_ch_train.py pattern for consistency
- `models/ml_node_selector.json` - Trained model weights and normalization parameters
  - feature_keys: ['degree', 'out_degree', 'in_degree', 'clustering_coefficient', 'avg_neighbor_degree']
  - y_mean: -8.08 (negative confirms edge difference metric)
  - y_std: 28.44
  - w1: [5, 16], b1: [16], w2: [16, 1], b2: [1]

## Decisions Made

- **Edge difference labels**: Use actual contraction benefit (original_edges - shortcuts) NOT degree proxy - required to achieve phase goal of improvement over baseline
- **Training diversity**: Generate samples from 4 graph types (grid, scale_free, geometric, road_like) to ensure model generalizes across structures
- **Sample size**: ~150 nodes per graph (1800 total) balances training quality with generation time
- **Model architecture**: 16 hidden units matches learned_ch_train.py, sufficient for this feature space
- **Follow existing patterns**: Match MLPOrderer pattern for model loading and feature_keys structure

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - training pipeline worked on first run.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

**Ready for next phase:**

- Training pipeline is complete and functional
- Model file (models/ml_node_selector.json) can be loaded by MLNodeSelector
- Edge difference metric correctly implemented (y_mean = -8.08 confirms this)
- MLNodeSelector from Plan 01 successfully loads and uses trained model

**No blockers or concerns.**

Next phase (02-03) can use this trained model for baseline vs ML comparison experiments.

---
*Phase: 02-ml-based-node-selection*
*Completed: 2026-02-04*
