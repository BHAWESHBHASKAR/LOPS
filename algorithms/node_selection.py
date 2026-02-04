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
from typing import Dict

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
