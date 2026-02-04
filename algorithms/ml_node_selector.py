"""
ML-based node selection for contraction hierarchies.

This module extends NodeSelector with ML-predicted node importance scores.
When a trained model is available, it uses learned feature combinations to
identify important nodes. Falls back to degree-based heuristics when the
model is unavailable.
"""

from typing import Set, Optional
import heapq

from algorithms.node_selection import NodeSelector
from algorithms.learned_ch_orderer import MLPOrderer
from algorithms.ml_features import extract_node_features


class MLNodeSelector(NodeSelector):
    """
    Selects high-importance nodes using ML-predicted scores or degree fallback.

    This class extends NodeSelector to support machine learning models for
    node importance prediction. When a model is provided, it uses learned
    feature combinations to score nodes. When no model is available, it
    gracefully falls back to degree-based selection.

    Attributes:
        graph: The graph to analyze
        model: Optional MLPOrderer model for scoring nodes
        _degree_cache: Inherited cache for degree computations

    Usage:
        >>> from algorithms.ml_node_selector import MLNodeSelector
        >>> from algorithms.graph_gen import GraphGenerator
        >>> g = GraphGenerator.scale_free_graph(100)
        >>> selector = MLNodeSelector(g, model_path="models/trained_model.json")
        >>> top_10 = selector.select_top_k(10)
        >>> print(f"Top 10 nodes: {top_10}")

        Without model (falls back to degree):
        >>> selector = MLNodeSelector(g)  # No model provided
        >>> top_10 = selector.select_top_k(10)  # Uses degree-based selection
    """

    def __init__(self, graph, model_path: Optional[str] = None):
        """
        Initialize MLNodeSelector with optional model.

        Args:
            graph: Graph object to analyze
            model_path: Optional path to trained model JSON file
                       If provided and valid, model will be used for scoring
                       If None or invalid, falls back to degree-based selection
        """
        super().__init__(graph)
        self.model = None

        # Try to load model if path provided
        if model_path is not None:
            try:
                self.model = MLPOrderer.load(model_path)
                print(f"Loaded ML model from {model_path}")
            except FileNotFoundError:
                print(f"Warning: Model file not found: {model_path}")
                print("Falling back to degree-based selection")
            except Exception as e:
                print(f"Warning: Failed to load model: {e}")
                print("Falling back to degree-based selection")

    def ml_score(self, node: int) -> float:
        """
        Get ML-predicted importance score for a node.

        If a model is available, extracts features and returns the predicted
        score. Otherwise, falls back to degree-based scoring.

        Args:
            node: Node ID to score

        Returns:
            Predicted importance score (float)

        Time complexity:
            - With model: O(degree) for feature extraction
            - Without model: O(m) on first call, O(1) if cached (same as degree())

        Raises:
            ValueError: If node ID is out of valid range

        Usage:
            >>> score = selector.ml_score(42)
            >>> print(f"Predicted importance: {score}")
        """
        # Validate node (inherits validation from degree())
        if node < 0 or node >= self.graph.nodes:
            raise ValueError(f"Invalid node: {node}. Valid range: [0, {self.graph.nodes - 1}]")

        # Fall back to degree if no model available
        if self.model is None:
            return float(self.degree(node))

        # Extract features for the node
        features = extract_node_features(self.graph, node)

        # Build feature vector in model's expected order
        feature_vector = [features[k] for k in self.model.feature_keys]

        # Return model prediction
        return self.model.score(dict(zip(self.model.feature_keys, feature_vector)))

    def select_top_k(self, k: int) -> Set[int]:
        """
        Select top k nodes using ML scores (or degree fallback).

        This method overrides the parent to use ML-predicted scores when
        available. Falls back to degree-based selection if no model.

        Args:
            k: Number of top nodes to select

        Returns:
            Set of k node IDs with highest ML scores or degrees

        Time complexity: O(n log k) where n = number of nodes
        - Uses heapq.nlargest() with ml_score or degree as key function

        Raises:
            ValueError: If k <= 0 or k > number of nodes

        Usage:
            >>> top_10 = selector.select_top_k(10)
            >>> print(f"Top 10 nodes: {top_10}")
        """
        if k <= 0:
            raise ValueError(f"k must be positive, got {k}")
        if k > self.graph.nodes:
            raise ValueError(f"k cannot exceed number of nodes ({self.graph.nodes}), got {k}")

        # Use ML score if model available, else degree
        scoring_fn = self.ml_score if self.model is not None else self.degree

        # Use heapq.nlargest for O(n log k) performance
        top_k_nodes = heapq.nlargest(k, range(self.graph.nodes), key=scoring_fn)

        return set(top_k_nodes)

    def select_top_k_percent(self, percentage: float) -> Set[int]:
        """
        Select top k% of nodes using ML scores (or degree fallback).

        Args:
            percentage: Fraction of nodes to select, in range (0, 1.0]

        Returns:
            Set of node IDs representing top percentage by ML score or degree

        Time complexity: O(n log k) where k = percentage * n

        Raises:
            ValueError: If percentage <= 0 or percentage > 1.0

        Usage:
            >>> top_5_percent = selector.select_top_k_percent(0.05)
            >>> print(f"Top 5% nodes: {top_5_percent}")
        """
        if percentage <= 0:
            raise ValueError(f"percentage must be positive, got {percentage}")
        if percentage > 1.0:
            raise ValueError(f"percentage cannot exceed 1.0 (100%), got {percentage}")

        # Calculate k from percentage, ensuring at least 1 node
        k = max(1, int(self.graph.nodes * percentage))

        # Delegate to select_top_k for actual selection
        return self.select_top_k(k)
