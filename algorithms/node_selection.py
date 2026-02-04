"""
Node selection infrastructure for contraction hierarchies.

This module provides the NodeSelector class for computing and caching node degrees
in graphs. Degree computation is foundational for ranking and selecting high-
importance nodes in contraction hierarchies.

Time complexities:
- out_degree(): O(1) - direct access to adjacency list
- in_degree(): O(m) where m = number of edges (scans all edges)
- degree(): O(m) on first call, O(1) on subsequent calls (cached)
"""

from dataclasses import dataclass, field
from typing import Dict, List, Tuple, Set
import heapq

from algorithms.base import Graph


@dataclass
class NodeSelector:
    """
    Selects high-importance nodes from a graph based on degree metrics.

    This class provides efficient degree computation with caching to avoid
    redundant expensive calculations. The degree() method computes total degree
    (out-degree + in-degree) which is the most meaningful metric for contraction
    hierarchies.

    Attributes:
        graph: The graph to analyze
        _degree_cache: Private cache mapping node -> total degree

    Usage:
        >>> from algorithms.node_selection import NodeSelector
        >>> from algorithms.graph_gen import GraphGenerator
        >>> g = GraphGenerator.grid_graph(10, 10)
        >>> ns = NodeSelector(g)
        >>> degree = ns.degree(5)  # Returns total degree of node 5
        >>> ns.clear_cache()  # Clear cache after graph modifications
    """

    graph: Graph
    _degree_cache: Dict[int, int] = field(default_factory=dict)

    def degree(self, node: int) -> int:
        """
        Get total degree of a node (out-degree + in-degree).

        The total degree is the most meaningful metric for contraction hierarchies
        as it captures both forward and backward connectivity.

        Args:
            node: Node ID to compute degree for

        Returns:
            Total degree (out_degree + in_degree)

        Time complexity:
            O(m) on first call where m = number of edges
            O(1) on subsequent calls (cached)

        Raises:
            ValueError: If node ID is out of valid range
        """
        if node < 0 or node >= self.graph.nodes:
            raise ValueError(f"Invalid node: {node}. Valid range: [0, {self.graph.nodes - 1}]")

        # Return cached value if available
        if node not in self._degree_cache:
            # Compute total degree: out-degree + in-degree
            # out_degree is O(1), in_degree is O(m) but only computed once
            out_deg = self.out_degree(node)
            in_deg = self.in_degree(node)
            self._degree_cache[node] = out_deg + in_deg

        return self._degree_cache[node]

    def out_degree(self, node: int) -> int:
        """
        Get out-degree of a node (number of outgoing edges).

        Args:
            node: Node ID to compute out-degree for

        Returns:
            Out-degree count

        Time complexity: O(1) - direct access to adjacency list length

        Raises:
            ValueError: If node ID is out of valid range
        """
        if node < 0 or node >= self.graph.nodes:
            raise ValueError(f"Invalid node: {node}. Valid range: [0, {self.graph.nodes - 1}]")

        return self.graph.out_degree(node)

    def in_degree(self, node: int) -> int:
        """
        Get in-degree of a node (number of incoming edges).

        Args:
            node: Node ID to compute in-degree for

        Returns:
            In-degree count

        Time complexity: O(m) where m = number of edges (scans all adjacency lists)

        Note: This method does not cache results. Use degree() for cached
              total degree computation, or cache in-degree values separately
              if needed for repeated access.
        """
        if node < 0 or node >= self.graph.nodes:
            raise ValueError(f"Invalid node: {node}. Valid range: [0, {self.graph.nodes - 1}]")

        return self.graph.in_degree(node)

    def clear_cache(self) -> None:
        """
        Clear the degree computation cache.

        Important: Call this method after modifying the graph (adding/removing
        edges or nodes) to ensure degree values are recomputed on next access.

        Usage:
            >>> selector.clear_cache()
            >>> degree = selector.degree(node)  # Recomputes with fresh data
        """
        self._degree_cache.clear()

    def rank_nodes_by_degree(self) -> List[Tuple[int, int]]:
        """
        Rank all nodes by their total degree in descending order.

        This method computes the degree for all nodes and returns them sorted
        from highest degree to lowest. Useful for identifying the most important
        nodes in the graph for contraction hierarchies.

        Returns:
            List of (node_id, degree) tuples sorted by degree descending.
            Example: [(42, 15), (17, 12), (3, 10), ...] where node 42 has
            the highest degree of 15.

        Time complexity: O(n log n) where n = number of nodes
        - Degree computation: O(n * m) on first call, O(n) if cached
        - Sorting: O(n log n)

        Usage:
            >>> from algorithms.node_selection import NodeSelector
            >>> from algorithms.graph_gen import GraphGenerator
            >>> g = GraphGenerator.scale_free_graph(100)
            >>> ns = NodeSelector(g)
            >>> ranked = ns.rank_nodes_by_degree()
            >>> print(f"Top 5 nodes: {ranked[:5]}")
            >>> # Output: [(42, 15), (17, 12), (3, 10), (8, 9), (55, 8)]
        """
        # Create list of (node, degree) tuples for all nodes
        node_degrees = [(node, self.degree(node)) for node in range(self.graph.nodes)]

        # Sort by degree in descending order (highest degree first)
        node_degrees.sort(key=lambda x: x[1], reverse=True)

        return node_degrees

    def select_top_k(self, k: int) -> Set[int]:
        """
        Select the top k nodes with highest degree.

        This method uses heapq.nlargest() for optimal O(n log k) performance
        when k << n. For k ≈ n, rank_nodes_by_degree() is more efficient.

        Args:
            k: Number of top nodes to select. Must satisfy 0 < k <= graph.nodes

        Returns:
            Set of k node IDs with highest degrees

        Time complexity: O(n log k) where n = number of nodes
        - Uses heapq.nlargest() which maintains a min-heap of size k
        - More efficient than O(n log n) full sorting when k << n

        Raises:
            ValueError: If k <= 0 or k > number of nodes

        Usage:
            >>> from algorithms.node_selection import NodeSelector
            >>> from algorithms.graph_gen import GraphGenerator
            >>> g = GraphGenerator.scale_free_graph(100)
            >>> ns = NodeSelector(g)
            >>> top_10 = ns.select_top_k(10)
            >>> print(f"Top 10 nodes: {top_10}")
            >>> # Output: {42, 17, 3, 8, 55, 12, 67, 23, 9, 31}

        Note:
            This is preferred over rank_nodes_by_degree()[:k] when k is small
            relative to n. Use rank_nodes_by_degree() when you need all nodes
            or a large fraction (e.g., k > n/10).
        """
        if k <= 0:
            raise ValueError(f"k must be positive, got {k}")
        if k > self.graph.nodes:
            raise ValueError(f"k cannot exceed number of nodes ({self.graph.nodes}), got {k}")

        # Use heapq.nlargest for O(n log k) performance
        # This is more efficient than sorted()[:k] when k << n
        top_k_nodes = heapq.nlargest(k, range(self.graph.nodes), key=self.degree)

        return set(top_k_nodes)

    def select_top_k_percent(self, percentage: float) -> Set[int]:
        """
        Select the top k% of nodes with highest degree.

        This method calculates k as a percentage of total nodes and selects
        the top k nodes. Uses max(1, ...) to avoid k=0 for small percentages.

        Args:
            percentage: Fraction of nodes to select, in range (0, 1.0]
                        Example: 0.05 for 5%, 0.10 for 10%, 1.0 for 100%

        Returns:
            Set of node IDs representing the top percentage of nodes by degree

        Time complexity: O(n log k) where k = percentage * n
        - Delegates to select_top_k() which uses heapq.nlargest()

        Raises:
            ValueError: If percentage <= 0 or percentage > 1.0

        Usage:
            >>> from algorithms.node_selection import NodeSelector
            >>> from algorithms.graph_gen import GraphGenerator
            >>> g = GraphGenerator.scale_free_graph(100)
            >>> ns = NodeSelector(g)
            >>> top_5_percent = ns.select_top_k_percent(0.05)
            >>> print(f"Top 5% nodes: {top_5_percent}")
            >>> # Output: {42, 17, 3, 8, 55} (5 nodes for 100-node graph)

        Note:
            Uses max(1, int(n * percentage)) to ensure at least 1 node is
            selected even for small percentages. For example, 0.5% of a
            100-node graph would select 1 node (not 0).
        """
        if percentage <= 0:
            raise ValueError(f"percentage must be positive, got {percentage}")
        if percentage > 1.0:
            raise ValueError(f"percentage cannot exceed 1.0 (100%), got {percentage}")

        # Calculate k from percentage, ensuring at least 1 node
        # Use max(1, ...) to avoid off-by-one where k=0 for small percentages
        k = max(1, int(self.graph.nodes * percentage))

        # Delegate to select_top_k for actual selection
        return self.select_top_k(k)
