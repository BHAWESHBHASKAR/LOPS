# Feature Landscape: Partial Contraction Hierarchies

**Domain:** Novel shortest-path preprocessing technique extending Contraction Hierarchies
**Researched:** 2026-02-04
**Overall Confidence:** MEDIUM (PCH is novel; research based on CH literature and existing codebase)

## Executive Summary

Partial Contraction Hierarchies (PCH) represents a **novel approach** to shortest-path preprocessing that occupies a continuum between bidirectional search (0% contracted) and full Contraction Hierarchies (100% contracted). The core innovation is **selective contraction**: contract only the most "important" nodes (typically 10-20% of nodes) while leaving the remainder for standard bidirectional search.

**Key differentiator:** PCH offers tunable tradeoff between preprocessing cost and query performance. Unlike full CH (O(n² log n) preprocessing), PCH targets O(d·n) where d << n is the number of contracted nodes.

**Research approach:** This feature analysis synthesizes:
- Existing full CH implementation in codebase
- Academic literature on Contraction Hierarchies (2008-2024)
- Node importance metrics from network science
- Empirical results from overlay CH experiments

## Table Stakes

Features **essential** for PCH to function correctly. Missing these = PCH doesn't work.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| **Node Importance Scoring** | Must identify which nodes to contract | Medium | Degree is simplest proxy; edge-difference from CH is better |
| **Selective Contraction** | Core innovation: contract subset, not all | Medium | Requires modified CH loop with early termination |
| **Hybrid Query Strategy** | Must query contracted nodes with CH, remainder with bidirectional | High | Two-phase query: CH-up + bidirectional completion |
| **Correctness Verification** | Exact shortest paths required | Low | Compare all results to Dijkstra baseline (existing infrastructure) |
| **Shortcut Creation** | Contracted nodes need shortcuts | Medium | Reuse existing CH shortcut logic with witness search |
| **Witness Path Search** | Required for shortcut correctness | High | Bottleneck in full CH; even more critical in PCH |
| **Bidirectional Search Fallback** | For non-contracted portion | Low | Already implemented (bidirectional_dijkstra in all_algorithms.py) |
| **Preprocessing Metrics** | Measure O(d·n) complexity target | Low | Track shortcuts added, preprocessing time, nodes contracted |
| **Graph Statistics** | Degree distribution, avg degree for importance | Low | Already computed in existing CH (_compute_avg_degree) |

**Implementation Notes:**
- Node importance can start with simple degree-based selection
- Edge-difference heuristic from existing CH (_compute_features) provides more sophisticated scoring
- Hybrid query must handle transition between contracted and uncontracted portions

## Differentiators

Features that give PCH **competitive advantage** over full CH and bidirectional search.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| **Tunable Contraction Percentage** | User controls preprocessing vs query tradeoff | Low | Parameter k ∈ [0, 1]; k=0 = bidirectional, k=1 = full CH |
| **O(d·n) Preprocessing Target** | Dramatically faster than full CH's O(n² log n) | High | Achievable if d ~ 0.1-0.2 (10-20% nodes contracted) |
| **Adaptive Node Selection** | Automatically identify high-value nodes to contract | High | Use ML-based orderer (learned_ch_orderer.py already exists) |
| **Regime Detection** | Automatically choose between PCH, CH, bidirectional | High | Based on graph characteristics (degree distribution, clustering) |
| **Dynamic Contraction Adjustment** | Increase contraction if queries too slow | Medium | Online learning: monitor query performance, contract more nodes |
| **Partial Re-preprocessing** | When graph changes, only reprocess affected nodes | High | Research gap: incremental updates not well-studied |
| **Centrality-Based Selection** | Contract nodes with high betweenness/centrality | Medium | Better than degree alone; requires O(n·m) to compute |
| **Multi-Level Hierarchy** | Contract top 10%, then next 10%, etc. | High | Creates continuum of more-powerful-but-more-expensive preprocessors |
| **Query-Dependent Optimization** | Contract nodes relevant to observed query patterns | High | If queries clustered, contract hotspots first |
| **Shortcut Reduction** | Limit shortcuts to avoid graph blowup | Medium | Full CH adds many shortcuts; PCH should be more selective |

**Competitive Positioning:**

| Approach | Preprocessing | Query Time | When PCH Wins |
|----------|--------------|------------|---------------|
| Bidirectional Dijkstra | 0 | Slow (O(√n log n)) | Large graphs with frequent updates |
| PCH (10-20%) | Fast (O(d·n)) | Medium (5-10× speedup) | Moderate query volume, dynamic graphs |
| Full CH | Very Slow (O(n² log n)) | Very Fast (100× speedup) | Static graphs, massive query volume |

## Anti-Features

Features to **explicitly avoid**. Common mistakes in CH domain.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| **Adaptive Contraction Without Theoretical Basis** | No guarantees; risks incorrectness | Use principled importance metrics (edge-difference, degree) |
| **Contracting Random Nodes** | Destores graph structure, no speedup | Contract high-degree/high-centrality nodes |
| **Query-Specific Caching Without Limit** | Memory explosion, cache invalidation nightmare | LRU cache with size limit; benchmark hit rate |
| **Full Graph Preprocessing "Just In Case"** | Defeats purpose; use full CH instead | Set strict contraction percentage; measure preprocessing time |
| **Ignoring Shortcut Overhead** | Too many shortcuts = slower queries | Limit shortcuts per contracted node; track shortcut growth |
| **Bidirectional Search on Contracted Graph** | Incorrect; violates CH ordering constraints | Use CH query algorithm on contracted portion, bidirectional on remainder |
| **Non-Admissible Heuristics** | Violates exact correctness requirement | Use only proven heuristics (Euclidean, landmarks) or none |
| **Untuned Parameter Sweeps** | "Let's try every k from 0 to 1" is not research | Identify structural justification for k based on graph properties |
| **Optimizing for Wall-Clock Time Only** | Platform-dependent, not reproducible | Optimize for nodes expanded; wall-clock is secondary |
| **Scaling Beyond Structural Justification** | Project constraint: graphs >1500 nodes need explicit justification | Stay under limit unless demonstrating clear regime discovery |

**Anti-Patterns from Codebase History:**

From ALGORITHM_COMPARISON.md and overlay experiments:
- **Overlay CH with simple BFS partitioning failed** on road-like graphs (<1k nodes)
- **Lesson:** Hierarchical approaches need high-quality partitioning to beat A*
- **PCH implication:** Don't add hierarchical complexity unless partition quality is justified

## Feature Dependencies

```
Graph Statistics → Node Importance Scoring → Selective Contraction → Hybrid Query Strategy
                                          ↓
                                    Shortcut Creation
                                          ↓
                                    Witness Search (bottleneck)
                                          ↓
                                    Correctness Verification
```

**Critical Dependencies:**
1. **Node importance must be computed before** contraction can begin
2. **Witness search dominates preprocessing time** (HIGH confidence - multiple sources confirm)
3. **Hybrid query requires both CH and bidirectional** implementations to coexist
4. **Correctness verification requires Dijkstra baseline** (already exists)

**Optimization Priority:**
1. Witness search optimization (biggest preprocessing bottleneck)
2. Node importance quality (determines which nodes to contract)
3. Hybrid query transition (determines query performance)

## MVP Recommendation

**For MVP (Phase 1), prioritize:**

### Table Stakes (Must Have)
1. **Degree-based node importance** - Simple, O(n) to compute
2. **Top-k% contraction** - Contract nodes with highest degree (k = 10%, 20% parameterized)
3. **Hybrid query** - CH-up on contracted nodes + bidirectional on remainder
4. **Correctness verification** - Reuse existing Dijkstra comparison
5. **Preprocessing metrics** - Measure nodes contracted, shortcuts added, time

### One Differentiator
6. **Tunable contraction percentage** - Allow k ∈ {0, 0.1, 0.2, ..., 1.0} to explore continuum

**Defer to Post-MVP:**
- **ML-based node ordering** (learned_ch_orderer.py exists but adds complexity)
- **Adaptive/online contraction adjustment** (research gap)
- **Centrality metrics** (betweenness requires O(n·m) computation)
- **Incremental updates** (literature sparse, high complexity)

**MVP Success Criteria:**
- PCH preprocessing is < 20% of full CH preprocessing time
- PCH queries achieve 5-10× speedup vs bidirectional Dijkstra
- 100% correctness on all test graphs
- Clear demonstration of continuum: bidirectional (k=0) < PCH (k=0.2) < CH (k=1.0)

## Feature Complexity Analysis

### Low Complexity (Quick Wins)
- **Degree-based importance:** Count edges per node, sort
- **Tunable percentage:** Single parameter k
- **Preprocessing metrics:** Counters and timers (already in CH)
- **Correctness verification:** Infrastructure exists

### Medium Complexity (Standard Implementation)
- **Edge-difference importance:** Reuse _compute_features from CH
- **Selective contraction loop:** Modify preprocess() with early termination
- **Shortcut creation:** Reuse _contract() and _add_shortcut() from CH
- **Hybrid query:** Two-phase query algorithm

### High Complexity (Research Gaps)
- **Witness search optimization:** Bottleneck, requires algorithmic innovation
- **Hybrid query transition:** Determining when to switch from CH to bidirectional
- **Adaptive node selection:** ML-based or centrality-based (learned_ch_orderer exists but untested)
- **Incremental updates:** Literature sparse, dynamic graphs not well-studied
- **Multi-level hierarchy:** Layered PCH (contract 10%, then another 10%, etc.)

## Critical Path to MVP

**Week 1-2: Foundation**
1. Implement degree-based node importance
2. Create selective contraction loop (contract top k%)
3. Add preprocessing metrics (time, shortcuts, nodes contracted)

**Week 3-4: Hybrid Query**
4. Implement hybrid query (CH-up on contracted + bidirectional on remainder)
5. Add correctness verification (compare to Dijkstra)

**Week 5-6: Tuning & Experiments**
6. Sweep k ∈ {0, 0.1, 0.2, 0.5, 1.0} on multiple graph types
7. Measure preprocessing time, query performance, correctness
8. Identify regime boundaries where PCH outperforms baselines

**Week 7-8: Optimization (if needed)**
9. Optimize witness search (if preprocessing bottleneck confirmed)
10. Improve node importance (edge-difference instead of degree)

## Research Gaps & Opportunities

### High-Confidence Gaps (Literature confirms)

**1. Witness Search Dominance (HIGH confidence)**
- Multiple sources: "Local searches to find witness paths have the biggest share of preprocessing time"
- **Opportunity:** Witness path optimization is critical research direction
- **PCH impact:** Partial contraction may reduce witness searches if nodes selected carefully

**2. Incremental Updates Scarcity (MEDIUM confidence)**
- Literature search: Only 2013 thesis on incremental CH updates
- **Opportunity:** PCH with partial re-preprocessing is novel research direction
- **Gap:** No established patterns for updating only contracted nodes

**3. Customizable CH (CCH) Relevance (MEDIUM confidence)**
- 2025 survey on CCH addresses dynamic edge weight changes
- **Opportunity:** PCH could incorporate CCH ideas for faster updates
- **Gap:** Unclear if CCH applies to partial contraction

### Low-Confidence Gaps (Need Phase-Specific Research)

**4. Optimal Contraction Percentage (LOW confidence)**
- No literature on "how many nodes to contract"
- **Research needed:** Empirical sweep across graph types
- **Hypothesis:** 10-20% is sweet spot, but needs validation

**5. Node Importance Metrics Comparison (LOW confidence)**
- Literature discusses degree, edge-difference, but no comparative study for PCH
- **Research needed:** Benchmark degree vs edge-difference vs ML-based vs centrality
- **Hypothesis:** Edge-difference > degree for shortcut reduction

**6. Hybrid Query Transition Point (LOW confidence)**
- No literature on querying partially contracted graphs
- **Research needed:** Algorithmic design for switching between CH and bidirectional
- **Hypothesis:** Query upward through contracted nodes, then bidirectional in remainder

## Sources

### High Confidence (Context7/Official Docs)
- **Existing codebase:** contraction_hierarchy.py (full CH implementation)
- **Existing codebase:** all_algorithms.py (bidirectional Dijkstra baseline)
- **Existing codebase:** overlay_ch.py (hierarchical approach, failed on small graphs)
- **ALGORITHM_COMPARISON.md:** Empirical results showing bidirectional Dijkstra = 1.51× speedup
- **MATHEMATICAL_ANALYSIS.md:** Complexity analysis of bidirectional search

### Medium Confidence (Academic Literature)
- **"Faster and Simpler Hierarchical Routing in Road Networks"** (Geisberger et al., 2008) - Original CH paper, 1,214 citations
- **"Parallel Contraction Hierarchies Construction on Road Networks"** (Chen et al., 2024) - Recent work on preprocessing optimization
- **"Weak Contraction Hierarchies Work!"** (Zeitz, 2013) - Explores partial preprocessing
- **"Customizable Contraction Hierarchies"** (Dibbelt et al., 2014) - 209 citations, three-phase workflow

### Low Confidence (WebSearch/Single Source)
- **"Key node identification for network topology"** (Nature Scientific Reports, 2024) - Node importance metrics
- **"Incremental Updated of Contraction Hierarchies"** (TCD thesis, 2013) - Dynamic updates
- **"On Optimal Preprocessing for CH"** (IWCTS 2012) - O(nh log D) preprocessing bound

### Contradictions & Ambiguities
- **PCH novelty:** Literature discusses "partial contraction" but in context of stopping CH early, not selective node contraction
- **Node importance:** Network science suggests centrality metrics, but CH community uses edge-difference; unclear which is better for PCH
- **Hybrid query:** No precedent for querying partially contracted graphs; algorithm design is open

## Open Questions for Phase-Specific Research

1. **What is the optimal contraction percentage k for different graph types?**
   - Research: Sweep k across [0, 1] on grids, road networks, random graphs
   - Expected: k* depends on degree distribution and clustering

2. **How should hybrid query transition between contracted and uncontracted portions?**
   - Research: Design query algorithm, benchmark alternatives
   - Expected: Upward search through contracted hierarchy, then bidirectional

3. **Can PCH beat A* on road-like graphs where overlay CH failed?**
   - Research: Compare PCH vs A* on 800-node road network
   - Expected: PCH wins when preprocessing cost not amortized

4. **What node importance metric minimizes shortcuts for given query speedup?**
   - Research: Compare degree, edge-difference, learned, centrality
   - Expected: Edge-difference balances shortcuts and query performance

5. **Does PCH enable effective incremental updates when graph changes?**
   - Research: Modify edge weights, measure re-preprocessing cost
   - Expected: Only affected nodes need re-contraction

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Table Stakes Features | HIGH | Based on CH fundamentals and existing codebase |
| Differentiators | MEDIUM | PCH is novel; some features speculative |
| Anti-Features | HIGH | Based on CH literature mistakes and codebase history |
| MVP Recommendation | MEDIUM | Degree-based approach conservative; hybrid query untested |
| Research Gaps | MEDIUM | Witness search confirmed bottleneck; other gaps need validation |

## Overall Assessment

**PCH Feature Space is Well-Defined:**
- Core features identified from CH literature
- Differentiators offer competitive advantage over baselines
- Anti-features provide clear "what not to build" guidance

**Research Needed:**
- Optimal contraction percentage (empirical sweep)
- Hybrid query algorithm design (no literature precedent)
- Node importance comparison (degree vs edge-difference vs learned)
- Incremental update feasibility (literature sparse)

**MVP is Feasible:**
- Table stakes features implementable with existing CH code
- Degree-based importance provides conservative starting point
- Hybrid query is main algorithmic challenge, but well-scoped

**Next Step:** Implement MVP and empirically identify regime boundaries.
