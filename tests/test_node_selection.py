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
