# Phase 2: ML-Based Node Selection - Context

**Gathered:** 2026-02-04
**Status:** Ready for planning

## Phase Boundary

Extend the NodeSelector class from Phase 1 with ML-predicted node importance scores. Train models on graph features to predict which nodes provide the highest contraction benefit. The system will compare ML-based selection against degree-based heuristics to identify regimes where ML provides meaningful improvement.

## Implementation Decisions

### Evaluation approach

**Primary metric: Query performance (nodes expanded)**
- Fewer nodes expanded during queries = better node selection
- This is the primary metric from the project's core value
- Secondary metrics: preprocessing time, shortcuts added, query time

**Success threshold: Meaningful gain (5-10%)**
- ML needs at least 5-10% improvement over degree-based to justify complexity
- Smaller improvements are likely noise or overfitting
- Threshold ensures we only use ML when it provides real value

**Validation approach: Holdout test graphs**
- Train ML model on one set of graphs
- Test on unseen graph types to ensure generalization
- Prevents overfitting to specific graph structures
- Example: Train on grid + scale-free, test on geometric + road networks

**Failure handling: Fall back to degree-based**
- Automatically revert to degree-based if ML underperforms
- No point using ML if it's not better
- Keep degree-based as robust baseline
- Track which graph types ML wins vs loses on

### Claude's Discretion

**ML model type and features:**
- Choice of model architecture (logistic regression, small neural network, gradient boosting, etc.)
- Graph features to engineer (degree centrality, betweenness, clustering coefficient, page rank, etc.)
- Model complexity and hyperparameter tuning

**Training data generation:**
- How to label nodes as "good for contraction" vs "bad for contraction"
- Number of training examples needed
- Graph types and sizes for training set

**Model selection strategy:**
- When to use ML vs degree-based during production queries
- Whether to train per-graph or use a universal model
- How to handle graph types not seen during training

**Measurement and benchmarking:**
- Specific benchmark graphs for evaluation
- Number of queries to average over for stable metrics
- Statistical significance testing for improvement claims

## Specific Ideas

No specific requirements — open to standard ML approaches for node importance prediction in graph algorithms.

## Deferred Ideas

None — discussion stayed within phase scope.

---

*Phase: 02-ml-based-node-selection*
*Context gathered: 2026-02-04*
