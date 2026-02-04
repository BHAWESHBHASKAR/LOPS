"""
Validation script for ML node selector vs degree-based baseline.

This script compares the ML-based node selection against the simple degree-based
baseline to verify that the ML model provides measurable improvement.
"""

import sys
import os

import numpy as np

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

from algorithms.ml_node_selector import MLNodeSelector
from algorithms.node_selection import NodeSelector
from algorithms.graph_gen import GraphGenerator


def simulate_contraction_benefit(graph, node: int) -> float:
    """
    Compute edge difference metric for a node (contraction benefit).

    Lower edge difference = lower contraction cost = higher priority.
    Edge difference = original_edges - shortcuts_added
    """
    neighbors = graph.edges.get(node, [])
    original_edges = len(neighbors)

    # Simulate shortcuts: for each pair of neighbors, check if direct edge exists
    shortcuts = 0
    neighbor_list = [n for n, _ in neighbors]

    for i in range(len(neighbor_list)):
        for j in range(i + 1, len(neighbor_list)):
            u = neighbor_list[i]
            v = neighbor_list[j]

            # Check if edge exists between u and v
            u_edges = graph.edges.get(u, [])
            if not any(target == v for target, _ in u_edges):
                shortcuts += 1

    # Edge difference: original edges removed minus shortcuts added
    # Negative means more shortcuts than original edges (high contraction cost)
    edge_difference = original_edges - shortcuts

    return float(edge_difference)


def compare_selection_methods(graph, model_path, k_percent=0.1):
    """
    Compare ML vs degree-based node selection.

    Args:
        graph: Graph to test on
        model_path: Path to trained ML model
        k_percent: Fraction of nodes to select (default 10%)

    Returns:
        Dict with comparison metrics
    """
    k = max(1, int(graph.nodes * k_percent))

    # ML-based selection
    ml_selector = MLNodeSelector(graph, model_path=model_path)
    ml_nodes = ml_selector.select_top_k(k)

    # Degree-based selection (baseline)
    degree_selector = NodeSelector(graph)
    degree_nodes = degree_selector.select_top_k(k)

    # Calculate metrics
    ml_avg_degree = np.mean([degree_selector.degree(n) for n in ml_nodes])
    degree_avg_degree = np.mean([degree_selector.degree(n) for n in degree_nodes])

    # Calculate contraction benefit (edge difference) - lower is better
    ml_avg_edge_diff = np.mean([simulate_contraction_benefit(graph, n) for n in ml_nodes])
    degree_avg_edge_diff = np.mean([simulate_contraction_benefit(graph, n) for n in degree_nodes])

    # Calculate clustering coefficients
    def get_clustering_coefficient(graph, node):
        """Compute local clustering coefficient."""
        neighbors = graph.edges.get(node, [])
        neighbor_set = set([n for n, _ in neighbors])
        degree = len(neighbor_set)

        if degree < 2:
            return 0.0

        # Count triangles
        triangles = 0
        for i, neighbor_a in enumerate(neighbor_set):
            for neighbor_b in list(neighbor_set)[i + 1:]:
                neighbor_a_edges = graph.edges.get(neighbor_a, [])
                if any(nb == neighbor_b for nb, _ in neighbor_a_edges):
                    triangles += 1

        possible_triangles = degree * (degree - 1) / 2.0
        return triangles / possible_triangles if possible_triangles > 0 else 0.0

    ml_avg_clustering = np.mean([get_clustering_coefficient(graph, n) for n in ml_nodes])
    degree_avg_clustering = np.mean([get_clustering_coefficient(graph, n) for n in degree_nodes])

    # Jaccard similarity (measures difference between selections)
    intersection = len(ml_nodes & degree_nodes)
    union = len(ml_nodes | degree_nodes)
    jaccard = intersection / union if union > 0 else 0.0

    # For contraction benefit: higher edge difference is better
    # (less negative = fewer shortcuts added during contraction)
    # Calculate improvement as relative improvement
    if degree_avg_edge_diff != 0:
        edge_diff_improvement = ((ml_avg_edge_diff - degree_avg_edge_diff) / abs(degree_avg_edge_diff)) * 100
    else:
        edge_diff_improvement = 0.0

    return {
        "k": k,
        "ml_avg_degree": ml_avg_degree,
        "degree_avg_degree": degree_avg_degree,
        "ml_avg_edge_diff": ml_avg_edge_diff,
        "degree_avg_edge_diff": degree_avg_edge_diff,
        "ml_avg_clustering": ml_avg_clustering,
        "degree_avg_clustering": degree_avg_clustering,
        "jaccard_similarity": jaccard,
        "edge_diff_improvement_pct": edge_diff_improvement,
        "ml_nodes": ml_nodes,
        "degree_nodes": degree_nodes,
    }


def validate_on_graph(graph_type, graph_params, model_path):
    """
    Run validation on a single graph type.

    Args:
        graph_type: String describing graph type
        graph_params: Dict with 'generator' key (callable) and 'nodes' key
        model_path: Path to trained model

    Returns:
        Dict with validation results
    """
    print(f"\n{'=' * 60}")
    print(f"Testing on {graph_type} ({graph_params['nodes']} nodes)")
    print('=' * 60)

    # Generate graph
    graph = graph_params["generator"]()

    # Run comparison
    metrics = compare_selection_methods(graph, model_path, k_percent=0.1)

    # Print results
    print(f"\nTop {metrics['k']} nodes ({int(0.1 * 100)}%):")
    print(f"  Average degree (ML):           {metrics['ml_avg_degree']:.4f}")
    print(f"  Average degree (Baseline):     {metrics['degree_avg_degree']:.4f}")
    print(f"\n  Edge difference (ML):          {metrics['ml_avg_edge_diff']:.4f}")
    print(f"  Edge difference (Baseline):    {metrics['degree_avg_edge_diff']:.4f}")
    print(f"  (higher is better = fewer shortcuts added)")
    print(f"\n  Clustering coefficient (ML):   {metrics['ml_avg_clustering']:.4f}")
    print(f"  Clustering coefficient (Base): {metrics['degree_avg_clustering']:.4f}")
    print(f"\n  Jaccard similarity:            {metrics['jaccard_similarity']:.4f}")
    print(f"  (0.0 = completely different, 1.0 = identical)")

    # Verdict based on edge difference (higher is better)
    if metrics['edge_diff_improvement_pct'] > 0:
        print(f"\n  ✓ ML improves edge difference by {metrics['edge_diff_improvement_pct']:.2f}%")
        print(f"  (ML selects nodes that add fewer shortcuts during contraction)")
    elif metrics['edge_diff_improvement_pct'] < 0:
        print(f"\n  ✗ Baseline improves edge difference by {-metrics['edge_diff_improvement_pct']:.2f}%")
    else:
        print(f"\n  = ML and baseline perform equally")

    if metrics['jaccard_similarity'] < 0.8:
        print(f"  ✓ ML selects different nodes than baseline (Jaccard < 0.8)")
    else:
        print(f"  ! ML selects similar nodes to baseline (Jaccard >= 0.8)")

    return metrics


def main():
    """Main validation routine."""
    model_path = "models/ml_node_selector.json"

    # Check model exists
    if not os.path.exists(model_path):
        print(f"Error: Model file not found: {model_path}")
        print("Please run tools/train_ml_node_selector.py first")
        return 1

    print("ML Node Selector Validation")
    print("=" * 60)
    print(f"Model: {model_path}")
    print("Comparing ML-based selection vs degree-based baseline\n")

    # Test on multiple graph types
    graph_types = [
        {
            "name": "Grid graph",
            "nodes": 100,
            "generator": lambda: GraphGenerator.grid_graph(10, 10, weighted=False, seed=42),
        },
        {
            "name": "Scale-free graph",
            "nodes": 200,
            "generator": lambda: GraphGenerator.scale_free_graph(200, edges_per_node=2, weighted=False, seed=42),
        },
        {
            "name": "Geometric graph",
            "nodes": 150,
            "generator": lambda: GraphGenerator.geometric_graph(150, radius=15.0, weighted=False, seed=42)[0],
        },
    ]

    results = []
    for graph_config in graph_types:
        metrics = validate_on_graph(
            graph_config["name"],
            graph_config,
            model_path
        )
        results.append(metrics)

    # Summary
    print(f"\n{'=' * 60}")
    print("SUMMARY")
    print('=' * 60)
    print(f"Graph types tested: {len(results)}")

    # Count improvements (based on edge difference improvement)
    improvements = [m['edge_diff_improvement_pct'] for m in results]
    ml_wins = sum(1 for imp in improvements if imp > 0)
    avg_improvement = np.mean(improvements)

    print(f"ML improves edge difference on {ml_wins}/{len(results)} graph types")
    print(f"Average edge difference improvement: {avg_improvement:+.2f}%")

    # Final verdict
    print(f"\n{'=' * 60}")
    if ml_wins >= len(results) * 2 / 3:
        print("✓ VERDICT: ML improves over baseline")
        print("The ML model selects nodes with better contraction cost (edge difference).")
        return 0
    else:
        print("! VERDICT: Mixed results")
        print("The ML model shows varying performance across graph types.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
