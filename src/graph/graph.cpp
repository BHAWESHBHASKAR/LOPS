/**
 * PHOTON Graph Implementation
 */

#include "photon/graph.hpp"
#include <algorithm>
#include <random>
#include <numeric>
#include <cmath>
#include <unordered_set>

namespace photon {

// =============================================================================
// GraphBuilder Implementation
// =============================================================================

GraphBuilder::GraphBuilder(size_t estimated_nodes, size_t estimated_edges) {
    temp_edges_.reserve(estimated_edges);
    temp_coords_.reserve(estimated_nodes);
}

GraphBuilder& GraphBuilder::add_node(NodeId id) {
    max_node_ = std::max(max_node_, static_cast<size_t>(id + 1));
    return *this;
}

GraphBuilder& GraphBuilder::add_edge(NodeId from, NodeId to, Weight weight) {
    temp_edges_.push_back({from, to, weight});
    max_node_ = std::max(max_node_, static_cast<size_t>(std::max(from, to) + 1));
    return *this;
}

GraphBuilder& GraphBuilder::add_undirected_edge(NodeId a, NodeId b, Weight weight) {
    add_edge(a, b, weight);
    add_edge(b, a, weight);
    return *this;
}

GraphBuilder& GraphBuilder::set_coordinate(NodeId node, float x, float y) {
    if (node >= temp_coords_.size()) {
        temp_coords_.resize(node + 1, {0, 0});
    }
    temp_coords_[node] = {x, y};
    return *this;
}

GraphBuilder& GraphBuilder::build_reverse(bool enable) {
    build_reverse_ = enable;
    return *this;
}

GraphBuilder& GraphBuilder::sort_by_weight(bool enable) {
    sort_by_weight_ = enable;
    return *this;
}

GraphBuilder& GraphBuilder::use_morton_order(bool enable) {
    use_morton_ = enable;
    return *this;
}

void GraphBuilder::compute_stats(Graph& g) {
    auto& stats = g.stats_;
    stats.num_nodes = g.num_nodes_;
    stats.num_edges = g.edges_.size();
    
    if (stats.num_nodes == 0) return;
    
    // Compute average degree and variance
    std::vector<size_t> degrees(stats.num_nodes);
    for (size_t i = 0; i < stats.num_nodes; ++i) {
        degrees[i] = g.out_degree(i);
    }
    
    double sum = 0, sum_sq = 0;
    for (size_t d : degrees) {
        sum += d;
        sum_sq += d * d;
    }
    
    stats.avg_degree = sum / stats.num_nodes;
    double variance = (sum_sq / stats.num_nodes) - (stats.avg_degree * stats.avg_degree);
    stats.degree_variance = std::sqrt(std::max(0.0, variance));
    
    // Estimate clustering coefficient (sample-based)
    size_t samples = std::min<size_t>(100, stats.num_nodes);
    size_t triangles = 0, triples = 0;
    
    std::mt19937 rng(42);
    for (size_t i = 0; i < samples; ++i) {
        NodeId v = rng() % stats.num_nodes;
        auto neighbors = g.out_edges(v);
        
        if (neighbors.size() < 2) continue;
        
        std::unordered_set<NodeId> neighbor_set;
        for (const auto& e : neighbors) {
            neighbor_set.insert(e.target);
        }
        
        for (const auto& e : neighbors) {
            for (const auto& e2 : g.out_edges(e.target)) {
                if (neighbor_set.count(e2.target)) {
                    ++triangles;
                }
            }
            triples += neighbor_set.size() - 1;
        }
    }
    
    stats.clustering_coefficient = triples > 0 ? 
        static_cast<float>(triangles) / triples : 0.0f;
    
    // Check if weighted
    bool all_unit = true;
    for (const auto& e : g.edges_) {
        if (std::abs(e.weight - 1.0f) > 0.001f) {
            all_unit = false;
            break;
        }
    }
    stats.is_weighted = !all_unit;
    
    // Has coordinates?
    stats.has_coordinates = !g.coordinates_.empty();
    
    // Rough diameter estimate (BFS from random node)
    size_t max_dist = 0;
    if (stats.num_nodes > 0) {
        std::vector<size_t> dist(stats.num_nodes, SIZE_MAX);
        std::vector<NodeId> queue;
        queue.reserve(stats.num_nodes);
        
        NodeId start = rng() % stats.num_nodes;
        dist[start] = 0;
        queue.push_back(start);
        
        for (size_t i = 0; i < queue.size(); ++i) {
            NodeId u = queue[i];
            for (const auto& e : g.out_edges(u)) {
                if (dist[e.target] == SIZE_MAX) {
                    dist[e.target] = dist[u] + 1;
                    max_dist = std::max(max_dist, dist[e.target]);
                    queue.push_back(e.target);
                }
            }
        }
    }
    stats.diameter_estimate = max_dist;
}

Graph GraphBuilder::build() {
    Graph g;
    g.num_nodes_ = max_node_;
    
    if (temp_edges_.empty()) {
        g.offsets_.resize(max_node_ + 1, 0);
        return g;
    }
    
    // Sort edges by source (for CSR)
    std::sort(temp_edges_.begin(), temp_edges_.end(), 
        [](const TempEdge& a, const TempEdge& b) {
            return a.from < b.from || (a.from == b.from && a.to < b.to);
        });
    
    // Build forward CSR
    g.offsets_.resize(max_node_ + 1, 0);
    g.edges_.reserve(temp_edges_.size());
    
    // Count edges per node
    for (const auto& e : temp_edges_) {
        ++g.offsets_[e.from + 1];
    }
    
    // Prefix sum
    for (size_t i = 1; i <= max_node_; ++i) {
        g.offsets_[i] += g.offsets_[i - 1];
    }
    
    // Fill edges
    g.edges_.resize(temp_edges_.size());
    std::vector<size_t> ptr = g.offsets_;
    for (const auto& e : temp_edges_) {
        g.edges_[ptr[e.from]++] = Edge{e.to, e.weight};
    }
    
    // Sort edges within each node by weight (optional)
    if (sort_by_weight_) {
        for (size_t i = 0; i < max_node_; ++i) {
            std::sort(
                g.edges_.begin() + g.offsets_[i],
                g.edges_.begin() + g.offsets_[i + 1],
                [](const Edge& a, const Edge& b) { return a.weight < b.weight; }
            );
        }
    }
    
    // Build reverse graph
    if (build_reverse_) {
        g.has_reverse_ = true;
        g.reverse_offsets_.resize(max_node_ + 1, 0);
        g.reverse_edges_.resize(temp_edges_.size());
        
        // Count incoming edges
        for (const auto& e : temp_edges_) {
            ++g.reverse_offsets_[e.to + 1];
        }
        
        // Prefix sum
        for (size_t i = 1; i <= max_node_; ++i) {
            g.reverse_offsets_[i] += g.reverse_offsets_[i - 1];
        }
        
        // Fill reverse edges
        ptr = g.reverse_offsets_;
        for (const auto& e : temp_edges_) {
            g.reverse_edges_[ptr[e.to]++] = Edge{e.from, e.weight};
        }
    }
    
    // Copy coordinates
    g.coordinates_ = std::move(temp_coords_);
    
    // Compute statistics
    compute_stats(g);
    
    return g;
}

Graph GraphBuilder::from_edges(
    std::span<const std::tuple<NodeId, NodeId, Weight>> edges,
    bool directed,
    bool build_reverse
) {
    GraphBuilder builder(0, edges.size() * (directed ? 1 : 2));
    builder.build_reverse(build_reverse);
    
    for (const auto& [from, to, weight] : edges) {
        if (directed) {
            builder.add_edge(from, to, weight);
        } else {
            builder.add_undirected_edge(from, to, weight);
        }
    }
    
    return builder.build();
}

Graph GraphBuilder::from_adjacency_list(
    const std::vector<std::vector<std::pair<NodeId, Weight>>>& adj,
    bool build_reverse
) {
    size_t total_edges = 0;
    for (const auto& neighbors : adj) {
        total_edges += neighbors.size();
    }
    
    GraphBuilder builder(adj.size(), total_edges);
    builder.build_reverse(build_reverse);
    
    for (size_t i = 0; i < adj.size(); ++i) {
        for (const auto& [to, weight] : adj[i]) {
            builder.add_edge(i, to, weight);
        }
    }
    
    return builder.build();
}

// =============================================================================
// Graph Generators
// =============================================================================

namespace generators {

Graph grid(size_t width, size_t height, bool diagonal, bool weighted) {
    GraphBuilder builder(width * height, width * height * (diagonal ? 8 : 4));
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> weight_dist(0.5f, 2.0f);
    
    auto id = [width](size_t x, size_t y) -> NodeId {
        return static_cast<NodeId>(y * width + x);
    };
    
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            NodeId node = id(x, y);
            builder.set_coordinate(node, x, y);
            
            // 4-connectivity
            const int dx4[] = {1, 0, -1, 0};
            const int dy4[] = {0, 1, 0, -1};
            
            for (int d = 0; d < 4; ++d) {
                int nx = x + dx4[d];
                int ny = y + dy4[d];
                if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                    Weight w = weighted ? weight_dist(rng) : 1.0f;
                    builder.add_edge(node, id(nx, ny), w);
                }
            }
            
            // 8-connectivity (diagonals)
            if (diagonal) {
                const int dx8[] = {1, 1, -1, -1};
                const int dy8[] = {1, -1, 1, -1};
                
                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx8[d];
                    int ny = y + dy8[d];
                    if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                        Weight w = weighted ? weight_dist(rng) * 1.414f : 1.414f;
                        builder.add_edge(node, id(nx, ny), w);
                    }
                }
            }
        }
    }
    
    return builder.build();
}

Graph random_graph(size_t num_nodes, float edge_probability, bool weighted) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> weight_dist(0.1f, 10.0f);
    
    size_t est_edges = num_nodes * num_nodes * edge_probability;
    GraphBuilder builder(num_nodes, est_edges);
    
    for (size_t i = 0; i < num_nodes; ++i) {
        for (size_t j = 0; j < num_nodes; ++j) {
            if (i != j && prob_dist(rng) < edge_probability) {
                Weight w = weighted ? weight_dist(rng) : 1.0f;
                builder.add_edge(i, j, w);
            }
        }
    }
    
    return builder.build();
}

Graph social_network(size_t num_nodes, size_t edges_per_node) {
    // Barabási-Albert preferential attachment model
    std::mt19937 rng(42);
    
    GraphBuilder builder(num_nodes, num_nodes * edges_per_node * 2);
    
    // Start with a small clique
    size_t m = std::min(edges_per_node, size_t(5));
    for (size_t i = 0; i < m; ++i) {
        for (size_t j = i + 1; j < m; ++j) {
            builder.add_undirected_edge(i, j, 1.0f);
        }
    }
    
    // Degree tracking for preferential attachment
    std::vector<size_t> degree(num_nodes, 0);
    for (size_t i = 0; i < m; ++i) {
        degree[i] = m - 1;
    }
    size_t total_degree = m * (m - 1);
    
    // Add remaining nodes
    for (size_t i = m; i < num_nodes; ++i) {
        std::unordered_set<NodeId> connected;
        
        // Connect to 'edges_per_node' existing nodes with preferential attachment
        while (connected.size() < edges_per_node && connected.size() < i) {
            // Random sampling based on degree
            std::uniform_int_distribution<size_t> dist(0, total_degree - 1);
            size_t target_weight = dist(rng);
            
            size_t cumsum = 0;
            for (size_t j = 0; j < i; ++j) {
                cumsum += degree[j] + 1;  // +1 to give even isolated nodes a chance
                if (cumsum > target_weight) {
                    if (connected.insert(j).second) {
                        builder.add_undirected_edge(i, j, 1.0f);
                        degree[i]++;
                        degree[j]++;
                        total_degree += 2;
                    }
                    break;
                }
            }
        }
    }
    
    return builder.build();
}

Graph road_network(size_t num_nodes, size_t avg_degree) {
    // Planar graph with some long-range connections (highways)
    std::mt19937 rng(42);
    
    // Place nodes randomly in 2D
    size_t grid_size = static_cast<size_t>(std::sqrt(num_nodes * 10));
    std::uniform_real_distribution<float> pos_dist(0.0f, grid_size);
    std::uniform_real_distribution<float> weight_jitter(0.8f, 1.2f);
    
    std::vector<std::pair<float, float>> positions(num_nodes);
    for (size_t i = 0; i < num_nodes; ++i) {
        positions[i] = {pos_dist(rng), pos_dist(rng)};
    }
    
    GraphBuilder builder(num_nodes, num_nodes * avg_degree);
    
    for (size_t i = 0; i < num_nodes; ++i) {
        builder.set_coordinate(i, positions[i].first, positions[i].second);
    }
    
    // Connect each node to nearest neighbors
    for (size_t i = 0; i < num_nodes; ++i) {
        // Find distances to all other nodes
        std::vector<std::pair<float, NodeId>> distances;
        for (size_t j = 0; j < num_nodes; ++j) {
            if (i == j) continue;
            float dx = positions[i].first - positions[j].first;
            float dy = positions[i].second - positions[j].second;
            float dist = std::sqrt(dx * dx + dy * dy);
            distances.push_back({dist, j});
        }
        
        // Sort by distance
        std::partial_sort(distances.begin(), 
            distances.begin() + std::min(avg_degree, distances.size()),
            distances.end());
        
        // Connect to nearest
        for (size_t k = 0; k < std::min(avg_degree, distances.size()); ++k) {
            Weight w = distances[k].first * weight_jitter(rng);
            builder.add_edge(i, distances[k].second, w);
        }
    }
    
    // Add some long-range "highway" connections
    size_t num_highways = num_nodes / 20;
    for (size_t i = 0; i < num_highways; ++i) {
        NodeId a = rng() % num_nodes;
        NodeId b = rng() % num_nodes;
        if (a != b) {
            float dx = positions[a].first - positions[b].first;
            float dy = positions[a].second - positions[b].second;
            float dist = std::sqrt(dx * dx + dy * dy);
            // Highways are faster (lower weight relative to distance)
            builder.add_undirected_edge(a, b, dist * 0.5f);
        }
    }
    
    return builder.build();
}

Graph maze(size_t width, size_t height) {
    // DFS maze generation (perfect maze with single solution)
    std::mt19937 rng(42);
    
    size_t num_nodes = width * height;
    std::vector<bool> visited(num_nodes, false);
    std::vector<std::pair<NodeId, NodeId>> edges;
    
    auto id = [width](size_t x, size_t y) -> NodeId {
        return static_cast<NodeId>(y * width + x);
    };
    
    auto coords = [width](NodeId n) -> std::pair<size_t, size_t> {
        return {n % width, n / width};
    };
    
    // DFS stack
    std::vector<NodeId> stack;
    stack.push_back(0);
    visited[0] = true;
    
    while (!stack.empty()) {
        NodeId curr = stack.back();
        auto [cx, cy] = coords(curr);
        
        // Find unvisited neighbors
        std::vector<NodeId> neighbors;
        const int dx[] = {1, 0, -1, 0};
        const int dy[] = {0, 1, 0, -1};
        
        for (int d = 0; d < 4; ++d) {
            int nx = cx + dx[d];
            int ny = cy + dy[d];
            if (nx >= 0 && nx < (int)width && ny >= 0 && ny < (int)height) {
                NodeId neighbor = id(nx, ny);
                if (!visited[neighbor]) {
                    neighbors.push_back(neighbor);
                }
            }
        }
        
        if (neighbors.empty()) {
            stack.pop_back();
        } else {
            // Random neighbor
            NodeId next = neighbors[rng() % neighbors.size()];
            visited[next] = true;
            edges.push_back({curr, next});
            stack.push_back(next);
        }
    }
    
    GraphBuilder builder(num_nodes, edges.size() * 2);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            builder.set_coordinate(id(x, y), x, y);
        }
    }
    
    for (const auto& [a, b] : edges) {
        builder.add_undirected_edge(a, b, 1.0f);
    }
    
    return builder.build();
}

Graph scale_free(size_t num_nodes, float gamma) {
    // Power-law degree distribution
    std::mt19937 rng(42);
    
    // Generate target degrees using inverse power law
    std::vector<size_t> target_degree(num_nodes);
    for (size_t i = 0; i < num_nodes; ++i) {
        // Degree ~ (i+1)^(-1/gamma) scaled
        float x = std::pow(i + 1.0f, -1.0f / gamma);
        target_degree[i] = std::max<size_t>(1, static_cast<size_t>(x * num_nodes / 10));
    }
    
    // Shuffle to randomize which nodes get high degree
    std::shuffle(target_degree.begin(), target_degree.end(), rng);
    
    size_t total_edges = 0;
    for (size_t d : target_degree) total_edges += d;
    
    GraphBuilder builder(num_nodes, total_edges);
    
    // Create stubs for each node
    std::vector<NodeId> stubs;
    for (size_t i = 0; i < num_nodes; ++i) {
        for (size_t j = 0; j < target_degree[i]; ++j) {
            stubs.push_back(i);
        }
    }
    
    // Shuffle and pair stubs
    std::shuffle(stubs.begin(), stubs.end(), rng);
    
    for (size_t i = 0; i + 1 < stubs.size(); i += 2) {
        NodeId a = stubs[i];
        NodeId b = stubs[i + 1];
        if (a != b) {
            builder.add_edge(a, b, 1.0f);
        }
    }
    
    return builder.build();
}

} // namespace generators

} // namespace photon
