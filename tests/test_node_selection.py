"""
Test suite for node selection functionality.

This module provides pytest fixtures for different graph types and tests
for degree computation and node ranking. The test fixtures support multiple
graph families to ensure generality of node selection algorithms.

Test coverage:
- Degree computation (out_degree, in_degree, total degree)
- Node ranking by degree
- Edge cases (single node, empty graph)
- Caching behavior
"""

import pytest
from algorithms.node_selection import NodeSelector
from algorithms.graph_gen import GraphGenerator


@pytest.fixture
def grid_graph():
    """
    Test fixture: 10x10 grid graph (100 nodes).

    Grid graphs have uniform degree distribution:
    - Corner nodes: degree 2
    - Edge nodes: degree 3
    - Inner nodes: degree 4

    Returns:
        Graph: A 10x10 grid graph with 100 nodes
    """
    return GraphGenerator.grid_graph(10, 10)


@pytest.fixture
def scale_free_graph():
    """
    Test fixture: scale-free graph with 100 nodes.

    Scale-free graphs have a power-law degree distribution with a few
    high-degree hub nodes and many low-degree nodes. This is ideal for
    testing node selection on graphs with heterogeneous degree distributions.

    Returns:
        Graph: A scale-free graph with 100 nodes generated using
               Barabási-Albert model
    """
    return GraphGenerator.scale_free_graph(100, edges_per_node=2)


@pytest.fixture
def geometric_graph():
    """
    Test fixture: geometric graph with 100 nodes.

    Geometric graphs connect nodes based on spatial proximity, creating
    clusters of interconnected nodes. This tests node selection on
    spatially-structured graphs.

    Returns:
        Tuple[Graph, Dict[int, Tuple[float, float]]]: A geometric graph
            with 100 nodes and their 2D positions
    """
    return GraphGenerator.geometric_graph(100, radius=15.0)


@pytest.fixture
def road_network_graph():
    """
    Test fixture: road-network-like graph with 100 nodes.

    Road-network-like graphs are sparse, clustered, and have moderate
    degree variation. This mimics real-world road networks where
    intersections have varying connectivity.

    Returns:
        Graph: A sparse, clustered graph with 100 nodes and average
               degree of approximately 4
    """
    return GraphGenerator.road_network_like(100, avg_degree=4)


# ============================================================================
# Degree Computation Tests
# ============================================================================

def test_degree_computation_scale_free(scale_free_graph):
    """
    Test degree computation on scale-free graph.

    Verifies:
    - degree() returns non-negative integers
    - out_degree() <= degree() (total >= out)
    - in_degree() <= degree()
    """
    selector = NodeSelector(scale_free_graph)

    for node in range(min(10, scale_free_graph.nodes)):  # Test first 10 nodes
        total_deg = selector.degree(node)
        out_deg = selector.out_degree(node)
        in_deg = selector.in_degree(node)

        # Verify non-negative
        assert total_deg >= 0, f"Node {node} has negative total degree: {total_deg}"
        assert out_deg >= 0, f"Node {node} has negative out-degree: {out_deg}"
        assert in_deg >= 0, f"Node {node} has negative in-degree: {in_deg}"

        # Verify total degree is sum of parts
        assert total_deg == out_deg + in_deg, \
            f"Node {node}: total={total_deg}, out={out_deg}, in={in_deg}"


def test_degree_caching(scale_free_graph):
    """
    Test that degree computation is cached properly.

    Verifies:
    - Second call returns cached value
    - clear_cache() empties the cache
    - After clear_cache(), degree is recomputed
    """
    selector = NodeSelector(scale_free_graph)

    # First call computes and caches
    node = 0
    degree1 = selector.degree(node)
    cache_size_1 = len(selector._degree_cache)

    # Second call should use cache
    degree2 = selector.degree(node)
    cache_size_2 = len(selector._degree_cache)

    assert degree1 == degree2, "Degree should be consistent"
    assert cache_size_1 == cache_size_2, "Cache should not grow on second call"

    # Clear cache
    selector.clear_cache()
    assert len(selector._degree_cache) == 0, "Cache should be empty after clear_cache()"

    # Recompute after cache clear
    degree3 = selector.degree(node)
    assert degree3 == degree1, "Degree should be same after cache clear"
    assert len(selector._degree_cache) == 1, "Cache should have one entry after recomputation"


@pytest.mark.parametrize("graph_fixture_name", ["grid_graph", "scale_free_graph", "geometric_graph"])
def test_degree_non_negative(request, graph_fixture_name):
    """
    Parametrized test: verify all nodes have non-negative degree across graph types.

    This test runs on grid, scale-free, and geometric graphs to ensure
    degree computation works correctly across different graph families.
    """
    # Get the fixture by name
    if graph_fixture_name == "geometric_graph":
        graph, _ = request.getfixturevalue(graph_fixture_name)
    else:
        graph = request.getfixturevalue(graph_fixture_name)

    selector = NodeSelector(graph)

    for node in range(graph.nodes):
        total_deg = selector.degree(node)
        assert total_deg >= 0, f"{graph_fixture_name}: Node {node} has negative degree: {total_deg}"


# ============================================================================
# Node Ranking Tests
# ============================================================================

def test_rank_nodes_by_degree(scale_free_graph):
    """
    Test node ranking by degree.

    Verifies:
    - Returns list of correct length
    - Sorted in descending order
    - First node has maximum degree
    """
    selector = NodeSelector(scale_free_graph)
    ranking = selector.rank_nodes_by_degree()

    # Check length
    assert len(ranking) == scale_free_graph.nodes, \
        f"Ranking should include all {scale_free_graph.nodes} nodes"

    # Check descending order
    for i in range(len(ranking) - 1):
        node_i, degree_i = ranking[i]
        node_j, degree_j = ranking[i + 1]
        assert degree_i >= degree_j, \
            f"Ranking not descending: node {node_i} has degree {degree_i}, " \
            f"node {node_j} has degree {degree_j}"

    # Check first node has max degree
    max_degree = max(selector.degree(n) for n in range(scale_free_graph.nodes))
    first_node, first_degree = ranking[0]
    assert first_degree == max_degree, \
        f"First node should have max degree {max_degree}, got {first_degree}"


def test_ranking_completeness(scale_free_graph):
    """
    Test that ranking includes all nodes exactly once.

    Verifies:
    - All nodes appear in ranking
    - Each node appears exactly once
    - No duplicate nodes
    """
    selector = NodeSelector(scale_free_graph)
    ranking = selector.rank_nodes_by_degree()

    # Extract node IDs from ranking
    ranked_nodes = [node for node, _ in ranking]

    # Create sets for comparison
    all_nodes = set(range(scale_free_graph.nodes))
    ranked_set = set(ranked_nodes)

    # Check completeness
    assert all_nodes == ranked_set, \
        f"Ranking missing nodes or has extras. Expected {all_nodes}, got {ranked_set}"

    # Check no duplicates
    assert len(ranked_nodes) == len(ranked_set), \
        "Ranking contains duplicate nodes"


# ============================================================================
# Edge Case Tests
# ============================================================================

def test_single_node_graph():
    """
    Test degree computation and ranking on single-node graph.

    Verifies:
    - degree() works for single node (should be 0)
    - rank_nodes_by_degree() returns [(0, 0)]
    """
    from algorithms.base import Graph

    graph = Graph(1)
    selector = NodeSelector(graph)

    # Test degree
    assert selector.degree(0) == 0, "Single node should have degree 0"
    assert selector.out_degree(0) == 0, "Single node should have out-degree 0"
    assert selector.in_degree(0) == 0, "Single node should have in-degree 0"

    # Test ranking
    ranking = selector.rank_nodes_by_degree()
    assert len(ranking) == 1, "Ranking should have one entry"
    assert ranking[0] == (0, 0), "Single node ranking should be [(0, 0)]"


def test_empty_graph():
    """
    Test behavior on empty graph (0 nodes).

    Verifies:
    - rank_nodes_by_degree() returns empty list
    - No errors on empty graph
    """
    from algorithms.base import Graph

    graph = Graph(0)
    selector = NodeSelector(graph)

    # Test ranking
    ranking = selector.rank_nodes_by_degree()
    assert len(ranking) == 0, "Empty graph should have empty ranking"


# ============================================================================
# Top-K Selection Tests (Absolute Count)
# ============================================================================

@pytest.mark.parametrize("k,expected_count", [
    (1, 1),
    (10, 10),
    (50, 50),
    (100, 100)
])
def test_select_top_k_count(scale_free_graph, k, expected_count):
    """
    Parametrized test: verify select_top_k() returns correct count.

    Tests various values of k from 1 to 100 (full graph).
    Verifies that exactly k nodes are selected.
    """
    selector = NodeSelector(scale_free_graph)
    selected = selector.select_top_k(k)

    assert len(selected) == expected_count, \
        f"Expected {expected_count} nodes, got {len(selected)}"


def test_select_top_k_correctness(scale_free_graph):
    """
    Test that select_top_k() returns the actual highest-degree nodes.

    Verifies:
    - Selected nodes match top k from ranking
    - Set comparison ensures exact match
    """
    selector = NodeSelector(scale_free_graph)
    k = 10

    # Select top 10 nodes
    selected = selector.select_top_k(k)

    # Get ranking and extract top 10
    ranking = selector.rank_nodes_by_degree()
    top_10_from_ranking = set(node for node, _ in ranking[:k])

    # Verify exact match
    assert selected == top_10_from_ranking, \
        f"Selected nodes {selected} don't match top 10 from ranking {top_10_from_ranking}"


def test_select_top_k_invalid(scale_free_graph):
    """
    Test that select_top_k() validates input parameters.

    Verifies:
    - k=0 raises ValueError
    - k=-1 raises ValueError
    - k=101 raises ValueError (graph has 100 nodes)
    """
    selector = NodeSelector(scale_free_graph)

    # Test k=0
    with pytest.raises(ValueError, match="k must be positive"):
        selector.select_top_k(0)

    # Test k=-1
    with pytest.raises(ValueError, match="k must be positive"):
        selector.select_top_k(-1)

    # Test k > number of nodes
    with pytest.raises(ValueError, match="k cannot exceed number of nodes"):
        selector.select_top_k(101)


# ============================================================================
# Top-K Selection Tests (Percentage)
# ============================================================================

@pytest.mark.parametrize("percentage,expected_count", [
    (0.05, 5),   # 5% of 100 = 5
    (0.10, 10),  # 10% of 100 = 10
    (0.15, 15),  # 15% of 100 = 15
    (0.20, 20),  # 20% of 100 = 20
    (1.00, 100)  # 100% of 100 = 100
])
def test_select_top_k_percent_count(scale_free_graph, percentage, expected_count):
    """
    Parametrized test: verify select_top_k_percent() returns correct count.

    Tests standard percentages: 5%, 10%, 15%, 20%, 100%
    Verifies that exactly k% of nodes are selected.
    """
    selector = NodeSelector(scale_free_graph)
    selected = selector.select_top_k_percent(percentage)

    assert len(selected) == expected_count, \
        f"Expected {expected_count} nodes for {percentage*100}%, got {len(selected)}"


@pytest.mark.parametrize("percentage", [0.05, 0.10, 0.15, 0.20])
def test_select_top_k_percent_correctness(scale_free_graph, percentage):
    """
    Parametrized test: verify select_top_k_percent() returns highest-degree nodes.

    Tests that selected nodes match top k% from ranking.
    Uses set comparison for exact match verification.
    """
    selector = NodeSelector(scale_free_graph)

    # Select top percentage of nodes
    selected = selector.select_top_k_percent(percentage)

    # Calculate expected k
    k = max(1, int(scale_free_graph.nodes * percentage))

    # Get ranking and extract top k
    ranking = selector.rank_nodes_by_degree()
    top_k_from_ranking = set(node for node, _ in ranking[:k])

    # Verify exact match
    assert selected == top_k_from_ranking, \
        f"Selected nodes {selected} don't match top {k} from ranking {top_k_from_ranking}"


def test_select_top_k_percent_invalid(scale_free_graph):
    """
    Test that select_top_k_percent() validates input parameters.

    Verifies:
    - percentage=0.0 raises ValueError
    - percentage=-0.1 raises ValueError
    - percentage=1.5 raises ValueError
    """
    selector = NodeSelector(scale_free_graph)

    # Test percentage=0.0
    with pytest.raises(ValueError, match="percentage must be positive"):
        selector.select_top_k_percent(0.0)

    # Test percentage=-0.1
    with pytest.raises(ValueError, match="percentage must be positive"):
        selector.select_top_k_percent(-0.1)

    # Test percentage > 1.0
    with pytest.raises(ValueError, match="percentage cannot exceed 1.0"):
        selector.select_top_k_percent(1.5)


# ============================================================================
# Cross-Graph Type Selection Tests
# ============================================================================

@pytest.mark.parametrize("graph_fixture", ["grid_graph", "scale_free_graph", "geometric_graph"])
@pytest.mark.parametrize("percentage", [0.05, 0.10, 0.15, 0.20])
def test_selection_works_on_all_graph_types(graph_fixture, percentage, request):
    """
    Parametrized test: verify selection works across all graph types.

    Tests node selection on grid, scale-free, and geometric graphs
    with multiple percentages (5%, 10%, 15%, 20%).

    Verifies:
    - Correct number of nodes selected
    - All selected nodes are valid (within graph bounds)
    """
    # Get the fixture by name
    if graph_fixture == "geometric_graph":
        graph, _ = request.getfixturevalue(graph_fixture)
    else:
        graph = request.getfixturevalue(graph_fixture)

    selector = NodeSelector(graph)
    selected = selector.select_top_k_percent(percentage)

    # Calculate expected count
    expected_count = max(1, int(graph.nodes * percentage))

    # Verify count
    assert len(selected) == expected_count, \
        f"{graph_fixture}: Expected {expected_count} nodes for {percentage*100}%, got {len(selected)}"

    # Verify all selected nodes are valid
    for node in selected:
        assert 0 <= node < graph.nodes, \
            f"{graph_fixture}: Invalid node ID {node} (valid range: [0, {graph.nodes - 1}])"


# ============================================================================
# Selection Correctness Tests
# ============================================================================

def test_selected_nodes_are_highest_degree(scale_free_graph):
    """
    Critical correctness test: verify selected nodes actually have highest degrees.

    This test proves that select_top_k_percent() correctly identifies
    the nodes with the highest degrees.

    Verifies:
    - Minimum degree in selected set >= maximum degree in non-selected set
    - This ensures ALL selected nodes have degree >= ALL non-selected nodes
    """
    selector = NodeSelector(scale_free_graph)
    percentage = 0.10  # Select top 10%

    # Select top 10% nodes
    selected = selector.select_top_k_percent(percentage)

    # Get degrees of selected and non-selected nodes
    selected_degrees = [selector.degree(node) for node in selected]
    all_nodes = set(range(scale_free_graph.nodes))
    non_selected = all_nodes - selected
    non_selected_degrees = [selector.degree(node) for node in non_selected]

    # Verify correctness
    min_selected_degree = min(selected_degrees) if selected_degrees else 0
    max_non_selected_degree = max(non_selected_degrees) if non_selected_degrees else 0

    assert min_selected_degree >= max_non_selected_degree, \
        f"Selection incorrect: min selected degree ({min_selected_degree}) < " \
        f"max non-selected degree ({max_non_selected_degree})"


def test_selection_is_subset_of_nodes(scale_free_graph):
    """
    Test that selected nodes are always a subset of graph nodes.

    Verifies:
    - All selected nodes are valid node IDs
    - No duplicate selections
    - Selection is a set (no duplicates)
    """
    selector = NodeSelector(scale_free_graph)

    for k in [1, 10, 50, 100]:
        selected = selector.select_top_k(k)

        # Verify all nodes are valid
        for node in selected:
            assert 0 <= node < scale_free_graph.nodes, \
                f"Invalid node {node} selected (valid range: [0, {scale_free_graph.nodes - 1}])"

        # Verify no duplicates (should be a set)
        assert len(selected) == len(set(selected)), \
            f"Selection contains duplicates for k={k}"


def test_selection_deterministic(scale_free_graph):
    """
    Test that selection is deterministic (same input → same output).

    Verifies:
    - Multiple calls with same parameters return same result
    """
    selector = NodeSelector(scale_free_graph)

    # Select top 10 nodes three times
    selected_1 = selector.select_top_k(10)
    selected_2 = selector.select_top_k(10)
    selected_3 = selector.select_top_k(10)

    # All selections should be identical
    assert selected_1 == selected_2 == selected_3, \
        "Selection is not deterministic"


# ============================================================================
# Error Handling Tests (Degree Methods)
# ============================================================================

def test_degree_invalid_node_id(scale_free_graph):
    """
    Test that degree() raises ValueError for invalid node IDs.

    Verifies:
    - node < 0 raises ValueError
    - node >= graph.nodes raises ValueError
    """
    selector = NodeSelector(scale_free_graph)

    # Test node < 0
    with pytest.raises(ValueError, match="Invalid node"):
        selector.degree(-1)

    # Test node >= graph.nodes
    with pytest.raises(ValueError, match="Invalid node"):
        selector.degree(scale_free_graph.nodes)


def test_out_degree_invalid_node_id(scale_free_graph):
    """
    Test that out_degree() raises ValueError for invalid node IDs.

    Verifies:
    - node < 0 raises ValueError
    - node >= graph.nodes raises ValueError
    """
    selector = NodeSelector(scale_free_graph)

    # Test node < 0
    with pytest.raises(ValueError, match="Invalid node"):
        selector.out_degree(-1)

    # Test node >= graph.nodes
    with pytest.raises(ValueError, match="Invalid node"):
        selector.out_degree(scale_free_graph.nodes)


def test_in_degree_invalid_node_id(scale_free_graph):
    """
    Test that in_degree() raises ValueError for invalid node IDs.

    Verifies:
    - node < 0 raises ValueError
    - node >= graph.nodes raises ValueError
    """
    selector = NodeSelector(scale_free_graph)

    # Test node < 0
    with pytest.raises(ValueError, match="Invalid node"):
        selector.in_degree(-1)

    # Test node >= graph.nodes
    with pytest.raises(ValueError, match="Invalid node"):
        selector.in_degree(scale_free_graph.nodes)


# ============================================================================
# Stress and Edge Case Tests
# ============================================================================

def test_very_large_graph_selection():
    """
    Test node selection on a very large graph (stress test).

    Verifies:
    - Selection works efficiently on large graphs
    - Correct count is returned
    - All selected nodes are valid
    """
    from algorithms.graph_gen import GraphGenerator

    # Create a large graph (1000 nodes)
    large_graph = GraphGenerator.scale_free_graph(1000, edges_per_node=2)
    selector = NodeSelector(large_graph)

    # Select top 5%
    selected = selector.select_top_k_percent(0.05)
    expected_count = max(1, int(1000 * 0.05))

    assert len(selected) == expected_count, \
        f"Expected {expected_count} nodes, got {len(selected)}"

    # Verify all nodes are valid
    for node in selected:
        assert 0 <= node < large_graph.nodes, \
            f"Invalid node {node} in selection"


def test_graph_with_isolated_nodes():
    """
    Test selection on graph with isolated nodes (degree=0).

    Verifies:
    - Isolated nodes are not selected (unless k=nodes)
    - Selection still works correctly
    """
    from algorithms.base import Graph

    # Create a graph with some isolated nodes
    graph = Graph(100)
    selector = NodeSelector(graph)

    # Add edges to create connected and isolated nodes
    for i in range(50):
        for j in range(i + 1, min(i + 5, 50)):
            graph.add_edge(i, j)

    # Nodes 50-99 are isolated (degree=0)
    # Select top 10 nodes - should all be from connected component
    selected = selector.select_top_k(10)

    # All selected nodes should have degree > 0
    for node in selected:
        assert selector.degree(node) > 0, \
            f"Node {node} has degree 0 but was selected"


def test_selection_on_directed_vs_undirected():
    """
    Test that selection works correctly on both directed and undirected graphs.

    Verifies:
    - Selection returns correct count for directed graphs
    - Selection returns correct count for undirected graphs
    - All selected nodes are valid in both cases
    """
    from algorithms.graph_gen import GraphGenerator

    # Test on scale-free (directed)
    directed_graph = GraphGenerator.scale_free_graph(100, edges_per_node=2)
    selector_directed = NodeSelector(directed_graph)
    selected_directed = selector_directed.select_top_k(10)

    assert len(selected_directed) == 10, "Directed graph selection failed"
    for node in selected_directed:
        assert 0 <= node < directed_graph.nodes, "Invalid node in directed selection"

    # Test on grid (effectively undirected in our implementation)
    undirected_graph = GraphGenerator.grid_graph(10, 10)
    selector_undirected = NodeSelector(undirected_graph)
    selected_undirected = selector_undirected.select_top_k(10)

    assert len(selected_undirected) == 10, "Undirected graph selection failed"
    for node in selected_undirected:
        assert 0 <= node < undirected_graph.nodes, "Invalid node in undirected selection"
