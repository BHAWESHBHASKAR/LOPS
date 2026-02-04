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
