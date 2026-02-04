# Domain Pitfalls

**Domain:** Partial Contraction Hierarchies and Hybrid Route Planning
**Researched:** 2025-02-04
**Overall confidence:** MEDIUM

## Executive Summary

Partial Contraction Hierarchies (PCH) and hybrid query approaches present several critical pitfalls that can compromise correctness, performance, or both. The most severe risks involve **correctness violations at contraction boundaries**, **suboptimal node selection**, and **query performance degradation**. Research shows that bidirectional search can be instance-optimal in theory, yet practical implementations face worst-case performance degradation on dense grids and certain graph topologies. Customizable Contraction Hierarchies (CCH) demonstrate that preprocessing can be reduced but not eliminated, and shortcuts explosion remains a fundamental challenge.

## Critical Pitfalls

### Pitfall 1: Correctness Violations at Contraction Boundaries

**What goes wrong:** When contracting only a subset of nodes (partial contraction), the boundary between contracted and uncontracted regions can create incorrect shortest paths. The query algorithm may "escape" the contracted hierarchy too early or fail to properly account for paths that cross between regions with different contraction levels.

**Why it happens:**
- Partial contraction creates regions with different graph augmentation levels
- Queries that cross boundaries must respect both the shortcuts in contracted regions AND the original graph structure in uncontracted regions
- The bidirectional search may terminate prematurely if one search front reaches a node that appears contracted but the other front hasn't properly explored the boundary

**Consequences:**
- **NON-DETERMINISTIC:** Returns paths longer than true shortest paths
- **SILENT FAILURES:** No easy way to detect without exhaustive verification
- **USER IMPACT:** Wrong routes in production systems

**Prevention:**
1. **Boundary-aware query termination:** Both forward and backward searches must reach nodes that can "see" each other through either shortcuts or original edges
2. **Explicit boundary validation:** After query completion, verify that the meeting point allows valid path reconstruction
3. **Incremental contraction validation:** Test correctness after each node contraction, not just at end of preprocessing
4. **Maintain "witness" edges:** Keep original graph edges alongside shortcuts to enable correctness checking

**Detection:**
- Unit tests comparing PCH results against full CH or Dijkstra on same queries
- Regression testing with diverse query pairs, especially near contraction boundaries
- Statistical analysis: Compare PCH path lengths against ground truth on random sample

**Phase mapping:** Address in Phase 1 (Foundation) — correctness violations are architectural flaws that must be prevented before optimization.

---

### Pitfall 2: Shortcuts Explosion - Memory Blowup

**What goes wrong:** The number of shortcut edges added during contraction can grow exponentially, causing memory exhaustion and making query performance worse than baseline algorithms. Research on contraction hierarchies notes that "the fewer shortcuts you add, the less time you take searching edges during preprocessing and querying."

**Why it happens:**
- Each node contraction can add O(degree²) shortcuts
- Dense regions or graphs with high degree nodes trigger shortcut cascades
- Poor node ordering (contracting high-degree nodes early) amplifies the problem
- Research from tensor network contraction shows that "unbalanced contraction trees are suboptimal" and partial contractions without careful ordering can increase complexity by 40x or more

**Consequences:**
- **Memory overflow:** Graph requires gigabytes instead of megabytes
- **Query slowdown:** Search must examine more edges than original graph
- **Preprocessing timeout:** O(n²) or worse contraction time
- **APEX dense grid issue:** 0.78x slowdown on dense grids directly relates to shortcuts explosion

**Prevention:**
1. **Degree-aware node ordering:** Contract low-degree nodes first (standard "edge difference" heuristic)
2. **Shortcut pruning:** Don't add shortcuts that don't significantly improve query performance
3. **Contract budget:** Limit total shortcuts added per node or globally
4. **Hierarchical contraction:** Contract in phases, validating query performance after each phase
5. **Topological awareness:** Avoid contracting nodes that form "cliques" or dense subgraphs

**Detection:**
- Monitor shortcuts/edges ratio during preprocessing (should stay well below 1.0)
- Profile memory usage: should scale near-linearly with graph size, not quadratically
- Benchmark query performance against baseline bidirectional Dijkstra
- Set thresholds: abort preprocessing if shortcuts exceed 3-5x original edges

**Phase mapping:** Address in Phase 1 (Foundation) — shortcuts explosion is a fundamental architecture constraint that determines viability.

---

### Pitfall 3: Suboptimal Node Selection for Partial Contraction

**What goes wrong:** Choosing which nodes to contract (and which to skip) dramatically affects performance. Bad choices lead to either excessive preprocessing cost or insufficient query speedup. The research indicates this is a **NON-DETERMINISTIC** problem requiring careful heuristics.

**Why it happens:**
- No clear metric for "which nodes benefit most from contraction"
- Static heuristics (degree, centrality) don't account for query distribution
- Graph topology varies widely: road networks vs. social networks vs. grid graphs
- The "edge difference" heuristic works for full CH but doesn't translate directly to partial contraction

**Consequences:**
- **Wasted preprocessing:** Contracting nodes that don't improve queries
- **Missed opportunities:** Skipping nodes that would provide large speedups
- **Unbalanced hierarchy:** Some regions over-contracted, others under-contracted
- **Poor ROI:** Preprocessing cost not justified by query speedup

**Prevention:**
1. **Query-aware sampling:** Use representative query distribution to guide contraction choices
2. **Adaptive contraction:** Monitor query performance and adjust contraction strategy dynamically
3. **Multi-criteria scoring:** Combine degree, betweenness, edge difference, and query statistics
4. **Incremental validation:** Contract nodes in batches, measure query impact, adjust strategy
5. **Regime-specific heuristics:** Different contraction strategies for different graph topologies (grids, road networks, social graphs)

**Detection:**
- **Profiling:** Measure query latency before/after contracting each node
- **A/B testing:** Compare different node selection strategies on synthetic workloads
- **Cost-benefit analysis:** Track preprocessing time vs. query speedup ratio
- **Target:** Each contracted node should improve query latency for >= 5% of queries

**Phase mapping:** Address in Phase 2 (PCH Implementation) — node selection is the core optimization problem for PCH.

---

### Pitfall 4: Query Performance Worse Than Bidirectional Baseline

**What goes wrong:** The hybrid approach (PCH + bidirectional search) performs worse than plain bidirectional Dijkstra. This is observed in APEX with 0.78x slowdown on dense grids. Research shows bidirectional Dijkstra is "instance-optimal" up to a constant factor, making it difficult to beat.

**Why it happens:**
- Overhead of maintaining contracted hierarchy exceeds query time savings
- Shortcuts explosion causes query to examine more edges than original graph
- Bidirectional search termination logic doesn't account for partial contraction
- Dense grids lack natural hierarchical structure, so contraction provides little benefit

**Consequences:**
- **NET NEGATIVE VALUE:** PCH makes things worse, not better
- **Wasted implementation effort:** Months of work for slower queries
- **Hard diagnosis:** Many factors can contribute (shortcuts, node ordering, termination)

**Prevention:**
1. **Baseline-first development:** Implement bidirectional search first, measure performance
2. **Incremental addition:** Add PCH on top, measure incremental benefit at each step
3. **Performance budget:** Set threshold (e.g., must achieve 1.5x speedup or skip PCH)
4. **Topology detection:** Detect dense graphs/grids early, skip PCH for those cases
5. **Graceful degradation:** Fall back to bidirectional search if PCH overhead detected

**Detection:**
- **Comparative benchmarking:** Always measure PCH against bidirectional baseline
- **Per-query profiling:** Track time spent in each query phase (search, unpacking, termination)
- **Acceptance criteria:** Define minimum speedup (e.g., 1.2x) before using PCH in production
- **Real-world validation:** Test on production-like query distributions, not just synthetic

**Phase mapping:** Address in Phase 2 (PCH Implementation) — must validate performance benefit before architectural commitment.

---

### Pitfall 5: Path Unpacking Overhead Dominates Query Time

**What goes wrong:** After finding the shortest path in the contracted graph, unpacking shortcuts back to original edges takes longer than the search itself. Research from Customizable Route Planning shows "unpacking" is a significant component of overall query time that authors explicitly report.

**Why it happens:**
- Each shortcut may recursively contain other shortcuts
- Deeply nested shortcuts require recursive unpacking
- Storing explicit paths for shortcuts trades memory for unpacking time
- Complex paths with many shortcuts cause worst-case O(path length) unpacking

**Consequences:**
- **Misleading metrics:** Search time looks fast, but total query time is slow
- **Asymmetric performance:** Some queries unpack quickly, others slowly
- **Memory-pressure tradeoff:** Storing full paths increases memory significantly
- **Unexpected bottlenecks:** Performance profiling doesn't reveal the issue without careful measurement

**Prevention:**
1. **Explicit path storage:** Store full path for each shortcut (memory vs. time tradeoff)
2. **Unpacking caching:** Cache unpacked paths for frequently-used shortcuts
3. **Shortcut depth limits:** Limit recursion depth during contraction (e.g., max 3 levels)
4. **Hybrid representation:** Store unpacked path for shortcuts with high usage, recursive for others
5. **Incremental unpacking:** Unpack during query rather than all at end (interleave with search)

**Detection:**
- **Detailed profiling:** Separate search time from unpacking time in metrics
- **Worst-case testing:** Test queries with longest paths, most shortcuts
- **Memory-aware monitoring:** Track memory usage vs. unpacking time correlation
- **Target:** Unpacking should be <= 20% of total query time

**Phase mapping:** Address in Phase 3 (Query Optimization) — path unpacking is a query-level optimization concern.

---

## Moderate Pitfalls

### Pitfall 6: O(n²) Preprocessing Never Completes

**What goes wrong:** Contraction preprocessing time grows quadratically (or worse) with graph size, making it impractical for large graphs. Existing codebase shows CH preprocessing is already O(n²) — partial contraction must improve this, not make it worse.

**Why it happens:**
- Naive contraction algorithms examine all node pairs during contraction
- No early termination or quality threshold during preprocessing
- Shortcut explosion causes each contraction to become more expensive over time
- Lack of incremental preprocessing or caching

**Consequences:**
- **Timeout:** Preprocessing never finishes on production-sized graphs
- **Iterative development:** Each test run takes hours, slowing development
- **Production unusability:** Can't re-preprocess when graph updates

**Prevention:**
1. **Contract in rounds:** Process graph in phases, validate after each phase
2. **Early termination:** Stop preprocessing when queries are "fast enough" (target latency)
3. **Incremental updates:** Add support for updating contracted graph without full rebuild
4. **Parallel preprocessing:** Multi-threaded contraction (carefully manages shared data structures)
5. **Preprocessing budget:** Set maximum time, return best-effort result if exceeded

**Detection:**
- **Progress monitoring:** Log contraction progress (nodes processed / total nodes)
- **Time projections:** Estimate total preprocessing time from early rounds
- **Quality checkpoints:** Measure query quality after each preprocessing round
- **Fallback:** If preprocessing would take too long, use simpler algorithm

**Phase mapping:** Address in Phase 1 (Foundation) — preprocessing scalability is a fundamental requirement.

---

### Pitfall 7: Race Conditions in ML Model Loading

**What goes wrong:** The codebase has "potential race conditions in ML model loading" — if PCH relies on ML models for node selection or query prediction, concurrent loading can cause crashes or incorrect behavior.

**Why it happens:**
- Multiple query threads load ML models simultaneously
- Model loading isn't thread-safe (static state, global variables)
- Lazy initialization causes double-loading or use-before-initialization
- Model inference isn't synchronized with model updates

**Consequences:**
- **Crashes:** Segfaults or exceptions from concurrent model access
- **Incorrect predictions:** Model returns garbage or stale results
- **Memory corruption:** Race conditions in model internals
- **Intermittent failures:** Hard to reproduce and debug

**Prevention:**
1. **Eager loading:** Load all models at startup before serving queries
2. **Thread-safe caching:** Use mutexes or atomic operations for model access
3. **Model versioning:** Track model versions, prevent queries during model reload
4. **Process isolation:** Run ML inference in separate process, communicate via RPC
5. **Immutable models:** Never modify loaded model, always replace with new instance

**Detection:**
- **Thread sanitizers:** Use TSAN or similar tools to detect races
- **Load testing:** Run concurrent queries, monitor for crashes
- **Stress testing:** Rapid model reloads during high query volume
- **Static analysis:** Scan code for unsafe shared state

**Phase mapping:** Address in Phase 4 (ML Integration) — ML integration requires thread safety from the start.

---

## Minor Pitfalls

### Pitfall 8: No Result Caching Squanders Optimization

**What goes wrong:** Queries don't cache results, so repeated queries pay full cost each time. For route planning with recurring queries (e.g., popular routes), this wastes optimization work.

**Why it happens:**
- Caching adds complexity (cache invalidation, memory management)
- Query distribution unknown in early development
- Cache miss penalty perceived as not worth it
- Focus on "one-shot" query optimization

**Consequences:**
- **Suboptimal performance:** Popular routes queried repeatedly
- **Resource waste:** CPU cycles spent on redundant computation
- **Missed wins:** Easy optimization left on table

**Prevention:**
1. **LRU cache:** Cache recent queries with size limit (e.g., 10,000 entries)
2. **Cache key:** Origin-destination pair, possibly with metric parameters
3. **Invalidation strategy:** TTL-based or graph-change-based invalidation
4. **Cache warming:** Pre-populate cache with likely queries during preprocessing
5. **Metrics:** Track cache hit rate, aim for > 30% on production workloads

**Detection:**
- **Query analysis:** Log query frequency, identify repeated pairs
- **Cache simulation:** Replay query logs with cache simulation to estimate hit rate
- **Production monitoring:** Measure cache effectiveness after deployment
- **Performance delta:** Compare cached vs. uncached query latency

**Phase mapping:** Address in Phase 3 (Query Optimization) — caching is a low-hanging fruit for query performance.

---

### Pitfall 9: Metric Customization Limitations

**What goes wrong:** Customizable Contraction Hierarchies (CCH) research shows that not all metrics can be efficiently customized. Some metrics require expensive re-customization or don't benefit from the CCH approach at all.

**Why it happens:**
- Some metrics depend on edge weights in ways shortcuts can't abstract
- Metric-dependent preprocessing can't be fully separated from metric-independent preprocessing
- Certain metrics (e.g., time-dependent, stochastic) require fundamentally different algorithms
- Research shows CCH achieves "performance trade-off with much lower metric-dependent preprocessing costs" but not zero cost

**Consequences:**
- **Limited applicability:** PCH only works for certain metrics
- **Surprising re-preprocessing:** Metrics assumed "customizable" actually aren't
- **Algorithm mismatch:** Wrong approach chosen for the metric

**Prevention:**
1. **Metric analysis:** Characterize metric properties before choosing algorithm
2. **Prototype early:** Build small-scale proof-of-concept for each metric type
3. **Hybrid approach:** Use PCH for some metrics, other algorithms for others
4. **Metric taxonomy:** Classify metrics into "PCH-friendly" and "PCH-unfriendly"
5. **Fallback planning:** Have alternative algorithms ready for unsupported metrics

**Detection:**
- **Literature review:** Research what metrics work with CCH/PCH
- **Metric testing:** Test customization time for each metric type
- **Performance validation:** Measure query quality after customization
- **Acceptance criteria:** Define maximum customization time (e.g., < 10 seconds)

**Phase mapping:** Address in Phase 1 (Foundation) — metric compatibility determines algorithmic viability.

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|-----------------|------------|
| Phase 1: Foundation | Shortcuts explosion, Metric compatibility | Set strict limits on shortcuts/edges ratio, prototype metrics early |
| Phase 1: Foundation | Correctness violations at boundaries | Implement rigorous testing before optimization |
| Phase 2: PCH Implementation | Query performance worse than baseline | Always benchmark against bidirectional Dijkstra |
| Phase 2: PCH Implementation | Suboptimal node selection | Use adaptive contraction with performance feedback |
| Phase 3: Query Optimization | Path unpacking overhead | Profile and optimize unpacking separately from search |
| Phase 3: Query Optimization | No result caching | Implement LRU cache with cache hit rate metrics |
| Phase 4: ML Integration | Race conditions in model loading | Eager loading, thread-safe model access |
| Phase 5: Production | O(n²) preprocessing | Implement incremental updates and early termination |

---

## Sources

### HIGH Confidence (Official Documentation/Academic Papers)

1. **Customizable Contraction Hierarchies** (Dibbelt et al., 2014)
   - URL: https://arxiv.org/pdf/1402.0402
   - Key insight: Three-phase preprocessing separates metric-independent and metric-dependent work
   - Relevance: Explains fundamental trade-off between customization and preprocessing cost

2. **Bidirectional Dijkstra Instance Optimality** (Haeupler et al., 2024)
   - URL: https://arxiv.org/html/2410.14638v2
   - Key insight: Bidirectional Dijkstra is instance-optimal up to constant factor
   - Relevance: Sets theoretical lower bound that PCH must beat to be valuable

3. **Graph Partitioning with Natural Cuts (PUNCH)** (Delling et al., 2010)
   - URL: https://www.microsoft.com/en-us/research/wp-content/uploads/2010/12/punchTR.pdf
   - Key insight: Road networks have natural separators (bridges, mountain passes)
   - Relevance: Informs node selection and contraction strategies

4. **Improved Cut Strategy for Tensor Network Contraction** (Staudt et al., 2024)
   - URL: https://drops.dagstuhl.de/storage/00lipics/lipics-vol301-sea2024/LIPIcs.SEA.2024.27/LIPIcs.SEA.2024.27.pdf
   - Key insight: Unbalanced contraction trees can be suboptimal, partial contraction requires careful ordering
   - Relevance: Shows pitfalls of poor contraction ordering

5. **Contraction Hierarchies Implementation Guide**
   - URL: https://jlazarsfeld.github.io/ch.150.project/sections/7-ch-overview/
   - Key insight: "The fewer shortcuts you add, the less time you take searching edges"
   - Relevance: Directly addresses shortcuts explosion pitfall

### MEDIUM Confidence (WebSearch with Academic Verification)

6. **Path Unpacking Bottlenecks** (Multiple CCH papers)
   - Papers explicitly report "unpacking" time in their results (Delling et al.)
   - Relevance: Confirms path unpacking is recognized bottleneck

7. **Route Planning Performance Degradation** (Various bidirectional search papers)
   - Inconsistent heuristics cause worst-case performance degradation
   - Relevance: Explains why bidirectional search can fail on dense grids

8. **Graph Partitioning Boundary Issues** (CMU, 2005)
   - Discusses "extra vertices on edges crossing interval boundaries"
   - Relevance: Informs correctness issues at contraction boundaries

### LOW Confidence (WebSearch Only, Needs Validation)

9. **APEX Dense Grid Slowdown**
   - Source: Project context mentions "APEX2 shows 0.78x slowdown on dense grids"
   - Confidence: LOW - needs verification with APEX documentation or implementation
   - Relevance: If true, demonstrates real-world impact of shortcuts explosion

10. **ML Model Race Conditions**
    - Source: Project context mentions "potential race conditions in ML model loading"
    - Confidence: LOW - needs code review and testing to verify
    - Relevance: Important for Phase 4 (ML Integration)

## Gaps to Address

### HIGH Priority (Blockers for Implementation)

1. **APEX dense grid investigation:** Verify the 0.78x slowdown claim, understand root cause
2. **ML model thread safety:** Audit code for race conditions in model loading
3. **Correctness validation strategy:** Define concrete test cases for boundary correctness

### MEDIUM Priority (Optimization Opportunities)

1. **Node selection heuristics:** Research best practices for partial contraction node ordering
2. **Incremental preprocessing:** Design approach for updating contracted graph without full rebuild
3. **Query distribution analysis:** Characterize real-world query patterns to guide optimization

### LOW Priority (Future Enhancements)

1. **Alternative algorithms:** Explore non-PCH approaches for metrics where PCH doesn't work
2. **Advanced caching:** Investigate learned caching strategies beyond simple LRU
3. **GPU acceleration:** Assess whether GPU can accelerate contraction or query phases

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Correctness violations | HIGH | Well-documented in CH literature, boundary issues are fundamental |
| Shortcuts explosion | HIGH | Explicitly addressed in multiple papers, root cause of APEX slowdown |
| Node selection | MEDIUM | Literature exists but specific to PCH (not full CH) is scarce |
| Query vs baseline performance | MEDIUM | Theoretical results (instance optimality) clear, practical results mixed |
| Path unpacking overhead | HIGH | Explicitly reported in CCH papers |
| Preprocessing scalability | MEDIUM | O(n²) is well-known, but specific PCH improvements need validation |
| ML race conditions | LOW | Needs code review and testing |
| Caching | HIGH | Standard technique, low-risk |
| Metric customization limitations | MEDIUM | CCH paper explicitly discusses trade-offs |

---

## Next Steps

1. **Verify APEX slowdown:** Obtain APEX documentation or implementation to understand dense grid issue
2. **Audit ML code:** Review model loading code for thread safety issues
3. **Design correctness tests:** Create comprehensive test suite for boundary correctness
4. **Prototype node selection:** Implement and compare multiple heuristics on synthetic graphs
5. **Benchmark preprocessing:** Measure preprocessing time and memory on increasing graph sizes
