"""
Unit tests for MLNodeSelector.

Tests ML-based node selection including model loading, fallback behavior,
and comparison with degree-based baseline.
"""

import unittest
import sys
import os

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

from algorithms.ml_node_selector import MLNodeSelector
from algorithms.node_selection import NodeSelector
from algorithms.graph_gen import GraphGenerator


class TestMLNodeSelector(unittest.TestCase):
    """Test cases for MLNodeSelector class."""

    def test_ml_node_selector_init(self):
        """Test MLNodeSelector initialization with valid model."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)

        # Check model is loaded
        self.assertIsNotNone(selector.model, "Model should be loaded successfully")
        # Check graph is set
        self.assertEqual(selector.graph.nodes, 100, "Graph should have 100 nodes")

    def test_ml_node_selector_fallback(self):
        """Test MLNodeSelector fallback to degree when model unavailable."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/nonexistent_model.json"

        selector = MLNodeSelector(graph, model_path=model_path)

        # Check model is None (fallback mode)
        self.assertIsNone(selector.model, "Model should be None when file not found")
        # Check ml_score returns degree value
        score = selector.ml_score(5)
        expected_degree = selector.degree(5)
        self.assertEqual(score, float(expected_degree), "ml_score should return degree when no model")

    def test_ml_score_with_model(self):
        """Test ML scoring with trained model."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)

        # Test multiple nodes
        for node in [0, 5, 10, 50, 99]:
            score = selector.ml_score(node)

            # Check score is float
            self.assertIsInstance(score, float, f"Score for node {node} should be float")
            # Check score is non-negative
            self.assertGreaterEqual(score, 0.0, f"Score for node {node} should be non-negative")

    def test_select_top_k(self):
        """Test selecting top k nodes."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)
        k = 10

        selected = selector.select_top_k(k)

        # Check exactly k nodes returned
        self.assertEqual(len(selected), k, f"Should select exactly {k} nodes")

        # Check all nodes are valid
        for node in selected:
            self.assertGreaterEqual(node, 0, "Node ID should be non-negative")
            self.assertLess(node, graph.nodes, "Node ID should be less than graph size")

        # Check no duplicates
        self.assertEqual(len(selected), len(set(selected)), "Selected nodes should have no duplicates")

    def test_select_top_k_percent(self):
        """Test selecting top k percent of nodes."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)
        percentage = 0.10  # 10%

        selected = selector.select_top_k_percent(percentage)

        # Calculate expected k
        expected_k = max(1, int(graph.nodes * percentage))

        # Check correct number of nodes selected
        self.assertEqual(len(selected), expected_k, f"Should select {expected_k} nodes for 10%")

        # Check all nodes are valid
        for node in selected:
            self.assertGreaterEqual(node, 0, "Node ID should be non-negative")
            self.assertLess(node, graph.nodes, "Node ID should be less than graph size")

    def test_ml_vs_degree_selection(self):
        """Test that ML selection differs from degree-based selection."""
        graph = GraphGenerator.scale_free_graph(200, edges_per_node=2, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        # ML-based selection
        ml_selector = MLNodeSelector(graph, model_path=model_path)
        ml_selected = ml_selector.select_top_k(20)

        # Degree-based selection
        degree_selector = NodeSelector(graph)
        degree_selected = degree_selector.select_top_k(20)

        # Calculate Jaccard similarity
        intersection = len(ml_selected & degree_selected)
        union = len(ml_selected | degree_selected)
        jaccard = intersection / union if union > 0 else 0.0

        # ML should differ from degree (Jaccard < 1.0)
        self.assertLess(jaccard, 1.0, "ML selection should differ from degree selection")
        # But both should return valid node sets
        self.assertEqual(len(ml_selected), 20, "ML should select 20 nodes")
        self.assertEqual(len(degree_selected), 20, "Degree should select 20 nodes")

    def test_ml_score_invalid_node(self):
        """Test ml_score with invalid node ID."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)

        # Test negative node ID
        with self.assertRaises(ValueError):
            selector.ml_score(-1)

        # Test node ID >= graph.nodes
        with self.assertRaises(ValueError):
            selector.ml_score(100)

    def test_select_top_k_invalid_k(self):
        """Test select_top_k with invalid k values."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)

        # Test k = 0
        with self.assertRaises(ValueError):
            selector.select_top_k(0)

        # Test k > graph.nodes
        with self.assertRaises(ValueError):
            selector.select_top_k(101)

    def test_select_top_k_percent_invalid_percentage(self):
        """Test select_top_k_percent with invalid percentages."""
        graph = GraphGenerator.grid_graph(10, 10, weighted=False, seed=42)
        model_path = "models/ml_node_selector.json"

        selector = MLNodeSelector(graph, model_path=model_path)

        # Test percentage = 0
        with self.assertRaises(ValueError):
            selector.select_top_k_percent(0.0)

        # Test percentage > 1.0
        with self.assertRaises(ValueError):
            selector.select_top_k_percent(1.5)

        # Test negative percentage
        with self.assertRaises(ValueError):
            selector.select_top_k_percent(-0.1)


if __name__ == "__main__":
    unittest.main()
