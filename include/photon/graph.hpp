/**
 * PHOTON Graph Data Structure
 * 
 * Cache-oblivious graph layout using Morton Z-order for spatial locality.
 * Supports both CSR (Compressed Sparse Row) and adjacency list views.
 */

#pragma once

#include "photon/core.hpp"
#include <vector>
#include <span>
#include <memory>
#include <algorithm>
#include <cmath>

namespace photon {

// =============================================================================
// Morton Code Utilities (for cache-oblivious layout)
// =============================================================================

namespace morton {

// Spread bits for 2D Morton code
inline constexpr uint32_t spread_bits(uint16_t x) noexcept {
    uint32_t v = x;
    v = (v | (v << 8)) & 0x00FF00FF;
    v = (v | (v << 4)) & 0x0F0F0F0F;
    v = (v | (v << 2)) & 0x33333333;
    v = (v | (v << 1)) & 0x55555555;
    return v;
}

// Encode 2D coordinates to Morton code
inline constexpr uint32_t encode_2d(uint16_t x, uint16_t y) noexcept {
    return spread_bits(x) | (spread_bits(y) << 1);
}

// Compact bits (reverse of spread)
inline constexpr uint16_t compact_bits(uint32_t v) noexcept {
    v &= 0x55555555;
    v = (v | (v >> 1)) & 0x33333333;
    v = (v | (v >> 2)) & 0x0F0F0F0F;
    v = (v | (v >> 4)) & 0x00FF00FF;
    v = (v | (v >> 8)) & 0x0000FFFF;
    return static_cast<uint16_t>(v);
}

// Decode Morton code to 2D coordinates
inline constexpr std::pair<uint16_t, uint16_t> decode_2d(uint32_t code) noexcept {
    return {compact_bits(code), compact_bits(code >> 1)};
}

} // namespace morton

// =============================================================================
// CSR Graph (Primary Storage)
// =============================================================================

class Graph {
public:
    // Default constructor
    Graph() = default;
    
    // Move-only (due to large data)
    Graph(Graph&&) noexcept = default;
    Graph& operator=(Graph&&) noexcept = default;
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;
    
    // ==========================================================================
    // Accessors
    // ==========================================================================
    
    [[nodiscard]] size_t num_nodes() const noexcept { return num_nodes_; }
    [[nodiscard]] size_t num_edges() const noexcept { return edges_.size(); }
    
    // Get outgoing edges for a node (CSR view)
    [[nodiscard]] std::span<const Edge> out_edges(NodeId node) const noexcept {
        if (node >= num_nodes_) return {};
        size_t start = offsets_[node];
        size_t end = offsets_[node + 1];
        return {edges_.data() + start, end - start};
    }
    
    // Get incoming edges (for bidirectional search)
    [[nodiscard]] std::span<const Edge> in_edges(NodeId node) const noexcept {
        if (!has_reverse_ || node >= num_nodes_) return {};
        size_t start = reverse_offsets_[node];
        size_t end = reverse_offsets_[node + 1];
        return {reverse_edges_.data() + start, end - start};
    }
    
    // Check if reverse graph is available
    [[nodiscard]] bool has_reverse() const noexcept { return has_reverse_; }
    
    // Get degree
    [[nodiscard]] size_t out_degree(NodeId node) const noexcept {
        return out_edges(node).size();
    }
    
    [[nodiscard]] size_t in_degree(NodeId node) const noexcept {
        return in_edges(node).size();
    }
    
    // Get statistics (cached)
    [[nodiscard]] const GraphStats& stats() const noexcept { return stats_; }
    
    // ==========================================================================
    // Coordinates (optional, for A* heuristic)
    // ==========================================================================
    
    [[nodiscard]] bool has_coordinates() const noexcept { 
        return !coordinates_.empty(); 
    }
    
    [[nodiscard]] std::pair<float, float> coordinate(NodeId node) const noexcept {
        if (node >= coordinates_.size()) return {0.0f, 0.0f};
        return coordinates_[node];
    }
    
    // Euclidean distance heuristic
    [[nodiscard]] Distance euclidean_distance(NodeId from, NodeId to) const noexcept {
        if (!has_coordinates()) return 0.0f;
        auto [x1, y1] = coordinate(from);
        auto [x2, y2] = coordinate(to);
        float dx = x2 - x1;
        float dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }
    
    // ==========================================================================
    // Raw access (for advanced algorithms)
    // ==========================================================================
    
    [[nodiscard]] const std::vector<size_t>& offsets() const noexcept { 
        return offsets_; 
    }
    
    [[nodiscard]] const std::vector<Edge>& edges() const noexcept { 
        return edges_; 
    }
    
    [[nodiscard]] const std::vector<size_t>& reverse_offsets() const noexcept { 
        return reverse_offsets_; 
    }
    
    [[nodiscard]] const std::vector<Edge>& reverse_edges() const noexcept { 
        return reverse_edges_; 
    }

private:
    friend class GraphBuilder;
    
    size_t num_nodes_ = 0;
    
    // Forward CSR
    std::vector<size_t> offsets_;      // Size: num_nodes + 1
    std::vector<Edge> edges_;          // Size: num_edges
    
    // Reverse CSR (for bidirectional search)
    bool has_reverse_ = false;
    std::vector<size_t> reverse_offsets_;
    std::vector<Edge> reverse_edges_;
    
    // Optional coordinates
    std::vector<std::pair<float, float>> coordinates_;
    
    // Cached statistics
    GraphStats stats_;
};

// =============================================================================
// Graph Builder (fluent API)
// =============================================================================

class GraphBuilder {
public:
    explicit GraphBuilder(size_t estimated_nodes = 0, size_t estimated_edges = 0);
    
    // Add node (optional, nodes are auto-created from edges)
    GraphBuilder& add_node(NodeId id);
    
    // Add edge
    GraphBuilder& add_edge(NodeId from, NodeId to, Weight weight = 1.0f);
    
    // Add undirected edge (adds both directions)
    GraphBuilder& add_undirected_edge(NodeId a, NodeId b, Weight weight = 1.0f);
    
    // Set coordinates
    GraphBuilder& set_coordinate(NodeId node, float x, float y);
    
    // Build options
    GraphBuilder& build_reverse(bool enable = true);
    GraphBuilder& sort_by_weight(bool enable = true);
    GraphBuilder& use_morton_order(bool enable = true);
    
    // Finalize and build graph
    [[nodiscard]] Graph build();
    
    // Build from edge list
    static Graph from_edges(
        std::span<const std::tuple<NodeId, NodeId, Weight>> edges,
        bool directed = true,
        bool build_reverse = true
    );
    
    // Build from adjacency list
    static Graph from_adjacency_list(
        const std::vector<std::vector<std::pair<NodeId, Weight>>>& adj,
        bool build_reverse = true
    );

private:
    struct TempEdge {
        NodeId from, to;
        Weight weight;
    };
    
    std::vector<TempEdge> temp_edges_;
    std::vector<std::pair<float, float>> temp_coords_;
    size_t max_node_ = 0;
    
    bool build_reverse_ = true;
    bool sort_by_weight_ = false;
    bool use_morton_ = false;
    
    void compute_stats(Graph& g);
};

// =============================================================================
// Graph Generators (for testing)
// =============================================================================

namespace generators {

// Grid graph with 4/8 connectivity
Graph grid(size_t width, size_t height, bool diagonal = true, bool weighted = false);

// Random Erdős–Rényi graph
Graph random_graph(size_t num_nodes, float edge_probability, bool weighted = true);

// Social network (Barabási–Albert preferential attachment)
Graph social_network(size_t num_nodes, size_t edges_per_node = 5);

// Road network (planar with highways)
Graph road_network(size_t num_nodes, size_t avg_degree = 4);

// Perfect maze (DFS maze generation)
Graph maze(size_t width, size_t height);

// Scale-free network (power-law degree distribution)
Graph scale_free(size_t num_nodes, float gamma = 2.5f);

} // namespace generators

} // namespace photon
