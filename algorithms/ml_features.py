"""
Feature extraction utilities for ML-based node selection.

This module provides efficient feature extraction for graph nodes, focusing on
O(1) complexity features that scale well to large graphs. Features are designed
to capture node importance for contraction hierarchies without expensive
centrality computations.
"""

from typing import Dict, List, Optional, Tuple

# Feature keys matching research recommendations
FEATURE_KEYS = [
    "degree",
    "out_degree",
    "in_degree",
    "clustering_coefficient",
    "avg_neighbor_degree",
]


def extract_node_features(
    graph,
    node: int,
    positions: Optional[Dict[int, Tuple[float, float]]] = None
) -> Dict[str, float]:
    """
    Extract features for a single node.

    Computes O(1) features that capture node importance and local structure:
    - degree: Total degree (out + in)
    - out_degree: Number of outgoing edges
    - in_degree: Number of incoming edges
    - clustering_coefficient: Local connectivity (triangles / possible)
    - avg_neighbor_degree: Average degree of neighbors
    - spatial_x, spatial_y: Node position (if positions provided)

    Args:
        graph: Graph object with out_degree() and in_degree() methods
        node: Node ID to extract features for
        positions: Optional dict mapping node -> (x, y) coordinates

    Returns:
        Dictionary mapping feature names to float values

    Time complexity: O(degree) for clustering and neighbor features
    Space complexity: O(1) - fixed-size feature dict

    Raises:
        ValueError: If node ID is out of valid range

    Usage:
        >>> from algorithms.ml_features import extract_node_features
        >>> from algorithms.graph_gen import GraphGenerator
        >>> g = GraphGenerator.grid_graph(10, 10)
        >>> features = extract_node_features(g, 5)
        >>> print(features)
        {'degree': 4.0, 'out_degree': 2.0, 'in_degree': 2.0,
         'clustering_coefficient': 0.0, 'avg_neighbor_degree': 4.0}
    """
    # Validate node ID
    if node < 0 or node >= graph.nodes:
        raise ValueError(f"Invalid node: {node}. Valid range: [0, {graph.nodes - 1}]")

    # Basic degree features
    out_deg = float(graph.out_degree(node))
    in_deg = float(graph.in_degree(node))
    total_degree = out_deg + in_deg

    # Get neighbors from adjacency list
    neighbors = graph.edges.get(node, [])
    neighbor_set = set([n for n, _ in neighbors])

    # Compute clustering coefficient (local connectivity)
    # Clustering = triangles / possible_triangles
    # where possible_triangles = degree * (degree - 1) / 2
    clustering = 0.0
    if total_degree >= 2:
        possible_triangles = total_degree * (total_degree - 1) / 2.0

        if possible_triangles > 0:
            # Count triangles: edges between neighbors
            triangles = 0
            for i, neighbor_a in enumerate(neighbor_set):
                for neighbor_b in list(neighbor_set)[i + 1:]:
                    # Check if edge exists between neighbor_a and neighbor_b
                    neighbor_a_edges = graph.edges.get(neighbor_a, [])
                    if any(nb == neighbor_b for nb, _ in neighbor_a_edges):
                        triangles += 1

            clustering = triangles / possible_triangles

    # Compute average neighbor degree
    avg_neighbor_deg = 0.0
    if neighbor_set:
        neighbor_degrees = [
            graph.out_degree(n) + graph.in_degree(n)
            for n in neighbor_set
        ]
        avg_neighbor_deg = sum(neighbor_degrees) / len(neighbor_degrees)

    # Build feature dict
    features = {
        "degree": total_degree,
        "out_degree": out_deg,
        "in_degree": in_deg,
        "clustering_coefficient": clustering,
        "avg_neighbor_degree": avg_neighbor_deg,
    }

    # Add spatial features if positions provided
    if positions and node in positions:
        x, y = positions[node]
        features["spatial_x"] = float(x)
        features["spatial_y"] = float(y)

    return features
