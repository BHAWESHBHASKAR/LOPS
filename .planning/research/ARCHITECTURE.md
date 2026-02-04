# Architecture Patterns: Partial Contraction Hierarchies

**Domain:** Partial Contraction Hierarchies (PCH) for exact shortest-path queries
**Researched:** 2026-02-04
**Overall confidence:** MEDIUM

## Executive Summary

Partial Contraction Hierarchies represent a hybrid approach between zero-preprocessing bidirectional search and full Contraction Hierarchies. The architecture centers around **selective node contraction** (targeting 10-20% high-degree nodes) combined with a **hybrid query strategy** that leverages both contracted overlay structure and standard bidirectional search.

Based on research of existing CH implementations, overlay architectures, and hybrid query approaches, the recommended architecture follows a **three-phase pipeline** with clear component boundaries:

1. **Node Selection Phase** — Identify high-degree/high-centrality nodes for contraction
2. **Partial Contraction Phase** — Contract selected subset, creating overlay shortcuts
3. **Hybrid Query Phase** — Query strategy adapts based on contracted/uncontracted regions

**Key architectural insight:** The challenge is not in implementing contraction or bidirectional search individually (both well-understood), but in **integrating them seamlessly** while maintaining correctness guarantees. The architecture must handle **mixed graph traversal** where some nodes are contracted (upward-only movement) while others remain uncontracted (standard bidirectional search).

## Recommended Architecture

### System Overview

```
Input Graph
     │
     ▼
┌─────────────────────────────────────────────────┐
│  Phase 1: Node Selection                        │
│  ┌─────────────────┐    ┌──────────────────┐   │
│  │ Degree Analyzer │───▶│ Centrality Calc  │   │
│  └─────────────────┘    └──────────────────┘   │
│           │                     │               │
│           └──────────┬──────────┘               │
│                      ▼                          │
│              ┌───────────────┐                  │
│              │ Node Selector │                  │
│              │ (Top K%)      │                  │
│              └───────────────┘                  │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│  Phase 2: Partial Contraction                   │
│  ┌─────────────────────────────────────────┐   │
│  │ Partial Contractor                      │   │
│  │  ┌──────────────┐   ┌────────────────┐ │   │
│  │  │ Orderer      │   │ Witness Search │ │   │
│  │  │ (Priority)   │   │ (Bounded Dijk) │ │   │
│  │  └──────────────┘   └────────────────┘ │   │
│  │         │                  │            │   │
│  │         └────────┬─────────┘            │   │
│  │                  ▼                      │   │
│  │         ┌────────────────┐              │   │
│  │         │ Shortcut Inserter│             │   │
│  │         └────────────────┘              │   │
│  └─────────────────────────────────────────┘   │
│                      │                          │
│                      ▼                          │
│              ┌───────────────┐                  │
│              │ Overlay Graph │                  │
│              │ (Original +   │                  │
│              │  Contracted)  │                  │
│              └───────────────┘                  │
└─────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────┐
│  Phase 3: Hybrid Query Engine                   │
│  ┌─────────────────────────────────────────┐   │
│  │ Query Strategy Selector                 │   │
│  │  ┌──────────────┐  ┌────────────────┐ │   │
│  │  │ Contraction  │  │ Path Character│ │   │
│  │  │ Analysis     │  │  Heuristic     │ │   │
│  │  └──────────────┘  └────────────────┘ │   │
│  └─────────────────────────────────────────┘   │
│           │                    │                │
│           ▼                    ▼                │
│  ┌──────────────────┐  ┌──────────────────┐   │
│  │ Overlay Search   │  │ Bidirectional    │   │
│  │ (Upward-only)    │  │ Dijkstra         │   │
│  └──────────────────┘  └──────────────────┘   │
│           │                    │                │
│           └────────┬───────────┘                │
│                    ▼                            │
│            ┌──────────────────┐                 │
│            │ Path Stitcher    │                 │
│            │ (Merge segments) │                 │
│            └──────────────────┘                 │
└─────────────────────────────────────────────────┘
                      │
                      ▼
                Output Path
```

## Component Boundaries

### Core Components

| Component | Responsibility | Inputs | Outputs | Communicates With |
|-----------|---------------|---------|----------|-------------------|
| **NodeSelector** | Selects subset of nodes for contraction | Graph, selection params | Set of contracted node IDs | DegreeAnalyzer, CentralityCalculator |
| **DegreeAnalyzer** | Computes degree statistics for all nodes | Graph (adjacency list) | Degree map: node → degree | NodeSelector |
| **CentralityCalculator** | Computes node importance metrics | Graph, optional positions | Centrality scores | NodeSelector |
| **PartialContractor** | Contracts selected nodes, adds shortcuts | Graph, selected nodes, ordering | Overlay graph with shortcuts | NodeSelector, Orderer, WitnessSearch |
| **Orderer** | Determines contraction order for selected nodes | Selected nodes, graph features | Ordered list for contraction | PartialContractor |
| **WitnessSearch** | Bounded Dijkstra to test shortcut necessity | Graph, source, target, max_dist | Boolean: witness exists? | PartialContractor |
| **ShortcutInserter** | Adds shortcut edges to graph | Graph, shortcut triples (u,v,w) | Updated graph | PartialContractor |
| **HybridQueryEngine** | Executes hybrid query strategy | Overlay graph, source, target | PathResult with metrics | QueryStrategySelector |
| **QueryStrategySelector** | Chooses query approach per query | Source, target, contraction metadata | Strategy choice | HybridQueryEngine |
| **OverlaySearch** | Upward-only search on contracted nodes | Overlay graph, source/target labels | Partial path | HybridQueryEngine |
| **BidirectionalSearch** | Standard bidirectional Dijkstra | Original graph, remaining nodes | Partial path | HybridQueryEngine |
| **PathStitcher** | Combines path segments from different strategies | Path segments from overlay/bidirectional | Complete path | HybridQueryEngine |

### Data Structures

```python
# Core graph representation
Graph = Dict[int, List[Tuple[int, float]]]  # node → [(neighbor, weight)]

# Contraction metadata
ContractedSet = Set[int]  # Nodes that have been contracted
RankMap = Dict[int, int]  # node → contraction order rank

# Overlay graph
OverlayGraph = Tuple[
    Graph,           # Original edges + shortcuts
    ContractedSet,   # Which nodes are contracted
    RankMap          # Node ordering
]

# Query context
QueryContext = DataClass(
    source: int,
    target: int,
    source_in_contracted: bool,  # Is source in contracted region?
    target_in_contracted: bool,  # Is target in contracted region?
    recommended_strategy: QueryStrategy
)

# Path segments (for stitching)
PathSegment = DataClass(
    nodes: List[int],
    distance: float,
    search_type: str,  # "overlay" or "bidirectional"
    entry_point: Optional[int],  # For stitching
    exit_point: Optional[int]
)
```

## Data Flow

### Preprocessing Flow

```
1. Graph Loading
   graph = load_graph(edges)
   reverse_graph = build_reverse(graph)

2. Node Selection Phase
   degrees = DegreeAnalyzer.compute(graph)
   centralities = CentralityCalculator.compute(graph, positions?)
   selected_nodes = NodeSelector.select_top_k(
       degrees, centralities, k_percentile=0.15
   )

3. Partial Contraction Phase
   ordered_nodes = Orderer.order_by_priority(
       selected_nodes, graph, reverse_graph
   )

   for node in ordered_nodes:
       shortcuts = WitnessSearch.find_needed_shortcuts(
           node, graph, reverse_graph, contracted_set
       )
       graph = ShortcutInserter.add_shortcuts(graph, shortcuts)
       contracted_set.add(node)
       rank_map[node] = current_rank

   overlay_graph = (graph, contracted_set, rank_map)

4. Store metadata
   - Entry/exit points for uncontracted regions
   - Boundary nodes between contracted/uncontracted
```

### Query Flow

```
1. Query Analysis
   query_ctx = QueryStrategySelector.analyze(
       overlay_graph, source, target
   )

   # Analysis checks:
   - Are source/target in contracted set?
   - Is shortest path likely via contracted region?
   - How many boundary crossings expected?

2. Strategy Selection

   CASE A: Both source and target in contracted region
   → Pure overlay search (bidirectional, upward-only)

   CASE B: Source contracted, target uncontracted
   → Hybrid: overlay search to boundary +
     bidirectional from boundary to target

   CASE C: Both uncontracted, path likely via contracted region
   → Hybrid: bidirectional to entry points +
     overlay through contracted region +
     bidirectional to exit

   CASE D: Both uncontracted, path unlikely via contracted region
   → Pure bidirectional search (fallback)

3. Query Execution (Hybrid Example)

   # Phase 1: Source to contracted region
   segment1 = BidirectionalSearch.search(
       original_graph,
       start=source,
       goals=boundary_entry_points
   )

   # Phase 2: Through contracted region
   segment2 = OverlaySearch.search(
       overlay_graph,
       start=segment1.exit_point,
       goal=segment3.entry_point,
       upward_only=True
   )

   # Phase 3: Contracted region to target
   segment3 = BidirectionalSearch.search(
       original_graph,
       start=boundary_exit_points,
       goal=target
   )

4. Path Stitching
   full_path = PathStitcher.stitch([segment1, segment2, segment3])
   metrics = compute_metrics(expanded_nodes, search_time)

5. Result Return
   return PathResult(
       distance=full_path.distance,
       path=full_path.nodes,
       nodes_expanded=sum(s.expanded for s in [segment1, segment2, segment3]),
       strategy=query_ctx.recommended_strategy
   )
```

## Integration with Existing Codebase

### New Files to Create

```
algorithms/
├── partial_ch.py              # Main PCH implementation (800-1000 lines)
│   ├── NodeSelector class
│   ├── DegreeAnalyzer class
│   ├── CentralityCalculator class
│   ├── PartialContractor class
│   ├── HybridQueryEngine class
│   └── PartialCH class (main interface)
├── pch_orderer.py             # Ordering strategy for PCH-specific
└── pch_query_strategy.py      # Query selection logic

tools/
├── pch_benchmark.py           # Benchmark PCH vs CH vs bidirectional
└── pch_ablation.py            # Ablation study (vary contraction %)
```

### Leveraging Existing Code

**From `algorithms/base.py`:**
- `Graph` class — Use as-is for graph representation
- `PathResult` dataclass — Use as-is for result format
- `ShortestPathAlgorithm` ABC — PartialCH should implement this

**From `algorithms/contraction_hierarchy.py`:**
- `_witness_search()` method — Adapt for partial contraction
- `_contract()` method — Use as template, only call on selected nodes
- `_add_shortcut()` method — Use directly
- `query()` bidirectional pattern — Adapt for hybrid strategy

**From `algorithms/baseline.py`:**
- `BidirectionalDijkstra` — Use as fallback for uncontracted regions
- Priority queue management patterns — Reuse

**From `algorithms/overlay_ch.py`:**
- `_partition_regions()` — Reference for region-based selection
- `_dijkstra_region()` — Adapt for boundary-to-boundary search
- `_dijkstra_overlay()` — Adapt for overlay search

### Main PCH Interface

```python
# algorithms/partial_ch.py

class PartialCH(ShortestPathAlgorithm):
    """
    Partial Contraction Hierarchies:
    - Contracts only top K% high-degree nodes
    - Hybrid query: overlay for contracted, bidirectional for rest
    """

    def __init__(
        self,
        contraction_percent: float = 0.15,  # Contract top 15%
        selection_criterion: str = "degree",  # or "centrality", "hybrid"
        query_mode: QueryMode = QueryMode.AUTO
    ):
        self.contraction_percent = contraction_percent
        self.selection_criterion = selection_criterion
        self.query_mode = query_mode

        # Components (composition over inheritance)
        self.node_selector = NodeSelector(selection_criterion)
        self.partial_contractor = PartialContractor()
        self.query_engine = HybridQueryEngine()

    def preprocess(self, graph: Graph) -> None:
        """Execute partial contraction preprocessing."""
        # Phase 1: Select nodes
        selected = self.node_selector.select_top_k(
            graph,
            k_percentile=self.contraction_percent
        )

        # Phase 2: Contract selected nodes
        self.overlay_graph = self.partial_contractor.contract(
            graph,
            nodes_to_contract=selected
        )

    def compute(self, source: int, target: int) -> PathResult:
        """Hybrid query execution."""
        return self.query_engine.query(
            self.overlay_graph,
            source,
            target,
            mode=self.query_mode
        )

    def name(self) -> str:
        return f"PartialCH({self.contraction_percent:.0%})"
```

## Patterns to Follow

### Pattern 1: Progressive Contraction with Ordering

**What:** Even when contracting only 10-20% of nodes, order matters significantly. High-degree nodes should be contracted last (they have most shortcuts).

**When:** All partial contraction implementations

**Why:** Contracting high-degree nodes early creates excessive shortcuts before we know which nodes matter most. Low-degree nodes are "cheap" to contract (few shortcuts).

**Example:**
```python
def order_selected_nodes(nodes: List[int], graph: Graph) -> List[int]:
    """
    Order selected nodes: low-degree first, high-degree last.
    This minimizes shortcuts by contracting "cheap" nodes first.
    """
    degrees = {n: graph.out_degree(n) + graph.in_degree(n) for n in nodes}
    return sorted(nodes, key=lambda n: degrees[n])  # Ascending degree
```

### Pattern 2: Upward-Only Search with Stopping Criterion

**What:** Overlay search must only traverse to higher-ranked nodes (upward-only). Use same stopping criterion as standard CH.

**When:** Query phase on contracted overlay

**Why:** Ensures correctness while exploiting hierarchy. Upward-only movement guarantees we don't "undo" shortcuts.

**Example:**
```python
def overlay_search_upward(
    overlay: OverlayGraph,
    source: int,
    target: int
) -> PathResult:
    """
    Bidirectional search, upward-only (rank[v] > rank[u]).
    """
    rank_map = overlay.rank_map
    graph = overlay.graph

    # Forward search
    pq_f = [(0, source)]
    dist_f = {source: 0}

    while pq_f:
        d, u = heapq.heappop(pq_f)

        for v, w in graph.get(u, []):
            # CRITICAL: Only move upward
            if rank_map[v] <= rank_map[u]:
                continue

            nd = d + w
            if nd < dist_f.get(v, float('inf')):
                dist_f[v] = nd
                heapq.heappush(pq_f, (nd, v))

    # ... backward search similar ...
    # ... stopping criterion: min(pq_f) + min(pq_b) >= best
```

### Pattern 3: Lazy Strategy Selection

**What:** Don't commit to pure overlay or pure bidirectional at start. Analyze query characteristics and switch strategies mid-execution if needed.

**When:** Hybrid query engine

**Why:** Some queries appear to need overlay but actually resolve quickly via bidirectional (and vice versa). Early pruning prevents wasted effort.

**Example:**
```python
def hybrid_query_with_fallback(
    overlay: OverlayGraph,
    source: int,
    target: int
) -> PathResult:
    """
    Start with overlay-biased strategy, fall back to bidirectional
    if overlay search is expanding too many nodes.
    """
    # Try overlay-biased first
    result = overlay_biased_search(overlay, source, target)

    # Fallback if overlay expanded > threshold
    if result.nodes_expanded > FALLBACK_THRESHOLD:
        return pure_bidirectional_search(
            overlay.original_graph,
            source,
            target
        )

    return result
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Naive Top-K Selection by Degree Alone

**What:** Select top K% nodes purely by degree without considering graph structure.

**Why bad:** Creates disconnected contraction regions. Two high-degree nodes far apart both get contracted, but intermediate nodes don't, leaving "islands" of contracted nodes.

**Consequences:**
- Hybrid query must constantly switch between overlay/bidirectional
- Path stitching becomes complex and error-prone
- Little benefit over pure bidirectional search

**Instead:** Use **connected component analysis** to ensure contracted nodes form coherent regions, or contract in spatial clusters (if positions available).

```python
# BAD: Pure top-k degree
def select_bad(graph, k=0.1):
    degrees = {n: degree(n) for n in graph.nodes}
    return sorted(nodes, key=degree)[:int(k * len(nodes))]

# GOOD: Clustered top-k
def select_good(graph, k=0.1):
    # Select high-degree nodes, then ensure connectivity
    candidates = sorted(nodes, key=degree)[:int(k * len(nodes))]
    regions = find_connected_components(graph.subgraph(candidates))
    return merge_nearby_regions(regions, max_distance=3)
```

### Anti-Pattern 2: Tight Coupling Between Contraction and Query

**What:** Query logic hardcoded to specific contraction strategy, or contractor hardcoded to specific selection criteria.

**Why bad:** Cannot experiment with different configurations without rewriting entire system. Violates single responsibility principle.

**Consequences:**
- Ablation studies require code duplication
- Cannot swap ordering strategies (degree vs centrality vs learned)
- Testing query logic independently requires mocking contractor

**Instead:** Use **dependency injection** and clear interfaces.

```python
# BAD: Hardcoded coupling
class PartialCH:
    def __init__(self):
        self.contractor = DegreeBasedContractor()  # Hardcoded

    def preprocess(self, graph):
        self.contractor.contract_top_15_percent(graph)  # Hardcoded

# GOOD: Interface-based composition
class PartialCH:
    def __init__(
        self,
        selector: NodeSelector,  # Interface
        contractor: PartialContractor  # Interface
    ):
        self.selector = selector
        self.contractor = contractor

    def preprocess(self, graph):
        selected = self.selector.select(graph, percentile=0.15)
        self.overlay = self.contractor.contract(graph, selected)
```

### Anti-Pattern 3: Re-computing Entry/Exit Points on Every Query

**What:** Hybrid query finds boundary nodes between contracted/uncontracted regions during query execution.

**Why bad:** Boundary analysis is expensive (graph traversal). Doing it per-query kills performance.

**Consequences:**
- Query time dominated by boundary analysis (not search)
- Defeats purpose of preprocessing (shifts work to query time)
- O(n) overhead per query instead of O(1) lookup

**Instead:** **Pre-compute and cache boundary metadata** during preprocessing.

```python
# BAD: Per-query boundary computation
def query_hybrid(source, target):
    # Runs every query!
    entry_points = find_boundary_nodes(source, contracted_set)
    exit_points = find_boundary_nodes(target, contracted_set)
    # ... rest of query ...

# GOOD: Pre-computed during preprocessing
class OverlayMetadata:
    entry_points: Dict[int, Set[int]]  # node → nearby boundary nodes
    exit_points: Dict[int, Set[int]]

def preprocess(graph, selected_nodes):
    # Run once during preprocessing
    metadata = compute_boundary_metadata(graph, selected_nodes)
    return OverlayGraph(graph, selected_nodes, metadata)

def query_hybrid(overlay, source, target):
    # O(1) lookup
    entries = overlay.metadata.entry_points.get(source, set())
    exits = overlay.metadata.exit_points.get(target, set())
```

### Anti-Pattern 4: Ignoring Correctness Verification

**What:** Assuming hybrid query is correct because overlay and bidirectional are individually correct.

**Why bad:** Path stitching between overlay and bidirectional segments can introduce subtle bugs (wrong node order, missed boundary conditions).

**Consequences:**
- Returns non-optimal paths while claiming optimality
- Hard to debug (intermittent failures on specific graph structures)
- Violates project's core "exact correctness" requirement

**Instead:** **Assert correctness on every query** during development, use randomized testing.

```python
def query_with_verification(
    overlay: OverlayGraph,
    source: int,
    target: int
) -> PathResult:
    result = hybrid_query(overlay, source, target)

    # ALWAYS verify against Dijkstra baseline in testing
    if DEBUG_MODE:
        dijkstra_result = dijkstra(overlay.original_graph, source, target)
        assert result.distance == dijkstra_result.distance, \
            f"Correctness violation: {result.distance} != {dijkstra_result.distance}"
        assert reconstruct_path(result) == dijkstra_result.path, \
            f"Path mismatch"

    return result
```

## Build Order Implications

### Suggested Implementation Sequence

**Phase 1: Node Selection (1-2 days)**
1. Implement `DegreeAnalyzer` — simple degree computation
2. Implement `CentralityCalculator` — start with betweenness centrality
3. Implement `NodeSelector` — top-k selection with percentile parameter
4. Test: Verify selected nodes are actually high-degree/centrality
5. Confidence: HIGH (well-understood graph algorithms)

**Integration point:** Add to `algorithms/partial_ch.py`, no dependencies yet

---

**Phase 2: Partial Contraction (2-3 days)**
1. Implement `Orderer` — priority-based ordering for selected nodes
2. Adapt `WitnessSearch` from existing CH — bounded Dijkstra
3. Implement `ShortcutInserter` — add edges to graph
4. Implement `PartialContractor` — orchestrate contraction loop
5. Test: Verify shortcuts are correct, graph remains connected
6. Confidence: MEDIUM (existing CH provides template, but partial adds complexity)

**Integration point:**
- Depends on: NodeSelector (Phase 1)
- Uses code from: `algorithms/contraction_hierarchy.py` (_witness_search, _contract)
- Produces: Overlay graph with contracted set metadata

**Critical test:** Contract 5 nodes, verify queries on contracted graph match Dijkstra

---

**Phase 3: Hybrid Query Strategy (3-4 days)**
1. Implement `QueryStrategySelector` — analyze query characteristics
2. Implement `OverlaySearch` — upward-only bidirectional search
3. Implement `PathStitcher` — combine path segments
4. Implement `HybridQueryEngine` — orchestrate strategy selection and execution
5. Test: Verify correctness on diverse graph types
6. Confidence: LOW-MEDIUM (novel integration challenge)

**Integration point:**
- Depends on: PartialContractor (Phase 2)
- Uses code from: `algorithms/baseline.py` (BidirectionalDijkstra)
- Uses code from: `algorithms/overlay_ch.py` (_dijkstra_overlay for reference)

**Critical test:** Query where source/target are both uncontracted, path goes through contracted region

---

**Phase 4: Optimization and Validation (2-3 days)**
1. Pre-compute boundary metadata during preprocessing
2. Implement fallback strategies (overlay → bidirectional)
3. Add correctness verification (assert against Dijkstra)
4. Benchmark against CH and bidirectional baselines
5. Ablation study: vary contraction percentage (5%, 10%, 15%, 20%)
6. Confidence: LOW until verified (may discover architecture issues)

**Integration point:**
- Depends on: All previous phases
- Produces: Final `PartialCH` class implementing `ShortestPathAlgorithm` ABC

**Critical validation:**
- 100% correctness verification on 100+ random queries
- Measure preprocessing time vs query speedup tradeoff
- Identify graph characteristics where PCH outperforms CH and bidirectional

---

**Total estimated time: 8-12 days** (assuming familiarity with existing CH implementation)

### Dependencies and Blockers

```
NodeSelector (Phase 1)
    │
    ▼
PartialContractor (Phase 2) ────────┐
    │                                │
    ▼                                │
HybridQueryEngine (Phase 3) ◀────────┘ (can start in parallel
    │                                │   once overlay structure
    ▼                                │   is defined)
Optimization (Phase 4) ──────────────┘
```

**Parallelization opportunities:**
- Phase 3 (query strategy) can start once overlay data structure is defined (Phase 2), even before PartialContractor is fully implemented
- CentralityCalculator (Phase 1) can be implemented later as enhancement to simple degree-based selection

**Critical path:** Phase 1 → Phase 2 → Phase 4 (Phase 3 can overlap with late Phase 2)

## Scalability Considerations

| Concern | At 100 nodes | At 1,000 nodes | At 10,000 nodes |
|---------|--------------|----------------|-----------------|
| **Node Selection** | O(n) degree scan, trivial | O(n) scan, <1ms | O(n) scan, ~10ms |
| **Partial Contraction** | Contract ~15 nodes, <100 shortcuts | Contract ~150 nodes, ~1K shortcuts | Contract ~1.5K nodes, ~10K shortcuts |
| **Preprocessing Time** | O(d·k) where d=avg degree, k=selected | ~10-50ms | ~100-500ms | ~1-5s |
| **Overlay Storage** | ~15-20% edge increase | ~15-20% edge increase | ~15-20% edge increase |
| **Query Time (contracted)** | ~2-5× faster than bidirectional | ~3-10× faster | ~5-15× faster |
| **Query Time (hybrid)** | ~1.5-3× faster than bidirectional | ~2-5× faster | ~3-8× faster |
| **Memory Overhead** | Store shortcuts + rank map | ~2-3× original graph | ~2-3× original graph |

**Key insight:** Partial CH scales linearly with graph size (same as bidirectional), unlike full CH which can approach O(n²) preprocessing on dense graphs. The 15-20% edge increase is bounded regardless of graph size.

## Roadmap Implications

Based on architectural analysis, suggested phase structure for roadmap:

### **Phase 1: Node Selection and Validation**
- **Focus:** Implement and validate node selection criteria
- **Deliverable:** Working `NodeSelector` with degree/centrality-based selection
- **Avoids:** Committing to specific contraction or query strategy
- **Research flags:** None (standard graph algorithms)

### **Phase 2: Partial Contraction Infrastructure**
- **Focus:** Build partial contraction pipeline, integrate with existing CH code
- **Deliverable:** Working `PartialContractor` that produces valid overlay graphs
- **Addresses:** Core novelty (partial vs full contraction)
- **Research flags:** May need deeper research on witness search bounds for partial contraction

### **Phase 3: Hybrid Query Integration**
- **Focus:** Implement hybrid query strategy, path stitching, correctness verification
- **Deliverable:** Working `HybridQueryEngine` with correctness guarantees
- **Addresses:** Main integration challenge
- **Research flags:** HIGH RISK — correctness of hybrid strategy is novel, may need iteration

### **Phase 4: Benchmarking and Regime Analysis**
- **Focus:** Compare PCH vs CH vs bidirectional across graph types
- **Deliverable:** Comprehensive benchmark results, regime characterization
- **Addresses:** Validation of core value proposition
- **Research flags:** May discover that PCH only benefits specific graph structures (requiring problem reformulation)

### **Phase 5: Optimization and Productionization**
- **Focus:** Pre-compute metadata, optimize hot paths, finalize API
- **Deliverable:** Production-ready `PartialCH` implementing `ShortestPathAlgorithm`
- **Addresses:** Integration with existing benchmark infrastructure
- **Research flags:** None (standard optimization)

**Phase ordering rationale:**
- Node selection first (no dependencies, validates assumptions)
- Partial contraction second (builds on selection, enables query work)
- Hybrid query third (most complex, needs overlay structure)
- Benchmarking fourth (needs working end-to-end system)
- Optimization last (premature optimization without data is waste)

## Sources

### HIGH Confidence (Official Documentation & Code)

1. **Existing CH Implementation** — `/Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/contraction_hierarchy.py`
   - Verified: `_contract()`, `_witness_search()`, `_add_shortcut()` methods
   - Verified: Bidirectional query pattern with upward-only movement
   - Verified: Priority-based node ordering

2. **Existing Overlay CH** — `/Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/overlay_ch.py`
   - Verified: Region-based partitioning approach
   - Verified: Two-level overlay architecture
   - Verified: Hybrid preprocessing (region-level contraction)

3. **Existing Bidirectional Dijkstra** — `/Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/baseline.py`
   - Verified: Bidirectional search implementation
   - Verified: Stopping criterion and meeting point logic

4. **Base Abstractions** — `/Users/bhaweshbhaskar/Desktop/isitpossible/algorithms/base.py`
   - Verified: `Graph`, `PathResult`, `ShortestPathAlgorithm` interfaces
   - Verified: Integration patterns for new algorithms

### MEDIUM Confidence (Verified Research Papers)

5. **"Customizable Contraction Hierarchies — A Survey"** (Bläsius et al., 2025)
   - arXiv: https://arxiv.org/pdf/2502.10519
   - Recent survey (2025) on CCH technical fundamentals
   - Node ordering strategies and preprocessing optimization

6. **"Parallel Contraction Hierarchies Can Be Efficient and..."** (Wan et al., ICS 2025)
   - Recent work on CH parallelization
   - Confirms active research area

7. **"Faster and Simpler Hierarchical Routing in Road Networks"** (Geisberger et al.)
   - 1,214+ citations, foundational CH work
   - Verified by multiple sources

8. **"Combining Hierarchical and Goal-Directed Speed-Up Techniques"**
   - Bidirectional query algorithm with multi-level overlay graphs
   - Upward-only search pattern
   - Verified by multiple CH implementation guides

### LOW Confidence (WebSearch Only — Needs Verification)

9. **DRNCS: Dual-Level Route Generation Model** (Li, ECML PKDD 2025)
   - Claims dual-level architecture inspired by CH
   - Mentions node contraction with multiple criteria
   - **Flag:** Preprint, not peer-reviewed yet
   - **Needs:** Verification of technical claims against official CH literature

10. **Degree centrality in node selection** (Various WebSearch results)
    - Claims that degree centrality is effective for low diffusion probabilities
    - **Flag:** Contradicted by some sources suggesting multi-criteria approaches
    - **needs:** Empirical validation in PCH context

### Gaps Requiring Phase-Specific Research

1. **Hybrid query correctness proofs**
   - Confidence: LOW
   - Reason: No literature found on formal correctness of mixing overlay + bidirectional search
   - Recommendation: Phase 3 must include rigorous testing, possibly formal proof

2. **Optimal contraction percentage for different graph types**
   - Confidence: LOW
   - Reason: Literature focuses on full CH, not partial
   - Recommendation: Phase 4 ablation study to empirically determine

3. **Boundary node selection for path stitching**
   - Confidence: MEDIUM-LOW
   - Reason: Some guidance from overlay CH, but not specific to partial contraction
   - Recommendation: Phase 3 may need experimentation with different strategies

4. **Performance tradeoffs: preprocessing cost vs query speedup**
   - Confidence: MEDIUM
   - Reason: General CH literature discusses this, but partial CH changes the equation
   - Recommendation: Phase 4 benchmarking to quantify

---

*Architecture research completed: 2026-02-04*
*Confidence assessment: MEDIUM overall (HIGH on component boundaries, MEDIUM on data flow, LOW on hybrid query integration details)*
