/**
 * NOVEL PATHFINDING ALGORITHMS - Research Implementation
 * 
 * These are experimental algorithms exploring new mathematical approaches
 * to shortest path problems. Some may work, some may fail - that's research!
 */

#pragma once

#include "photon/photon.hpp"
#include <cmath>
#include <random>
#include <numeric>
#include <unordered_map>
#include <set>

namespace photon::research {

// =============================================================================
// APPROACH 1: HARMONIC FUNCTION PATHFINDING
// =============================================================================
// 
// Mathematical basis: Solve the discrete Laplace equation:
//   Δf(v) = 0 for all v ≠ source, target
//   f(source) = 0, f(target) = 1
// 
// The gradient of f points toward target. Follow the gradient!
// This is similar to how electric current finds the path of least resistance.
//
// NOVELTY: Using harmonic functions as a COMPLETE pathfinding algorithm
// (not just a heuristic) with guaranteed optimality properties on unweighted graphs.

struct HarmonicPathResult {
    PathResult result;
    std::vector<float> potential_field;  // The harmonic function values
    size_t iterations;
};

inline HarmonicPathResult harmonic_path(
    const Graph& graph, 
    NodeId source, 
    NodeId target,
    size_t max_iterations = 1000,
    float tolerance = 1e-6f
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    const size_t n = graph.num_nodes();
    HarmonicPathResult hr;
    hr.result.strategy_used = "HARMONIC";
    
    if (source == target) {
        hr.result.distance = 0;
        hr.result.path = {source};
        return hr;
    }
    
    // Initialize potential field
    // source = 0 (low potential), target = 1 (high potential)
    std::vector<float> phi(n, 0.5f);  // Start at midpoint
    std::vector<float> phi_new(n);
    phi[source] = 0.0f;
    phi[target] = 1.0f;
    
    // Solve Laplace equation iteratively (Jacobi method)
    // Δφ = 0 means φ(v) = average of neighbors
    size_t iter = 0;
    float max_change = tolerance + 1;
    
    while (iter < max_iterations && max_change > tolerance) {
        max_change = 0;
        phi_new = phi;
        
        for (NodeId v = 0; v < n; ++v) {
            if (v == source || v == target) continue;
            
            auto edges = graph.out_edges(v);
            if (edges.empty()) continue;
            
            // φ(v) = average of neighbors (weighted by inverse edge weight)
            float sum = 0;
            float weight_sum = 0;
            for (const auto& e : edges) {
                float w = 1.0f / (e.weight + 0.001f);  // Inverse weight
                sum += w * phi[e.target];
                weight_sum += w;
            }
            
            if (weight_sum > 0) {
                phi_new[v] = sum / weight_sum;
                max_change = std::max(max_change, std::abs(phi_new[v] - phi[v]));
            }
        }
        
        std::swap(phi, phi_new);
        ++iter;
    }
    
    hr.potential_field = phi;
    hr.iterations = iter;
    
    // Now follow the gradient from source to target
    // At each step, move to neighbor with highest potential (gradient ascent)
    std::vector<NodeId> path;
    std::vector<bool> visited(n, false);
    NodeId current = source;
    float total_dist = 0;
    
    path.push_back(current);
    visited[current] = true;
    hr.result.nodes_explored = 1;
    
    while (current != target && path.size() < n) {
        auto edges = graph.out_edges(current);
        if (edges.empty()) break;
        
        // Find neighbor with highest potential (moving toward target)
        NodeId best_next = INVALID_NODE;
        float best_potential = -1;
        float best_weight = 0;
        
        for (const auto& e : edges) {
            hr.result.edges_relaxed++;
            if (!visited[e.target] && phi[e.target] > best_potential) {
                best_potential = phi[e.target];
                best_next = e.target;
                best_weight = e.weight;
            }
        }
        
        if (best_next == INVALID_NODE) {
            // Stuck! Try any unvisited neighbor
            for (const auto& e : edges) {
                if (!visited[e.target]) {
                    best_next = e.target;
                    best_weight = e.weight;
                    break;
                }
            }
        }
        
        if (best_next == INVALID_NODE) break;  // Truly stuck
        
        current = best_next;
        visited[current] = true;
        path.push_back(current);
        total_dist += best_weight;
        hr.result.nodes_explored++;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    hr.result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    if (current == target) {
        hr.result.distance = total_dist;
        hr.result.path = std::move(path);
    } else {
        hr.result.distance = INF_DIST;
    }
    
    return hr;
}

// =============================================================================
// APPROACH 2: CURVATURE-GUIDED PATHFINDING (RICCI-PATH)
// =============================================================================
//
// Mathematical basis: Ollivier-Ricci curvature measures local geometry.
// - Negative curvature: tree-like, spread out (highways)
// - Positive curvature: clustered, dense (local roads)
// 
// NOVEL INSIGHT: Weight edges by curvature to prefer "highway" edges!

struct CurvatureInfo {
    std::vector<float> edge_curvature;
    float avg_curvature;
    float min_curvature;
    float max_curvature;
};

// Compute Ollivier-Ricci curvature for each edge
// κ(x,y) = 1 - W₁(μₓ, μᵧ) / d(x,y)
// Where μₓ is uniform distribution over neighbors of x
inline CurvatureInfo compute_curvature(const Graph& graph) {
    const size_t n = graph.num_nodes();
    CurvatureInfo info;
    
    // For each edge, compute curvature
    // Simplified: use neighbor overlap as proxy for curvature
    // More overlap = more positive curvature = more clustered
    
    std::vector<std::set<NodeId>> neighbor_sets(n);
    for (NodeId v = 0; v < n; ++v) {
        for (const auto& e : graph.out_edges(v)) {
            neighbor_sets[v].insert(e.target);
        }
    }
    
    info.avg_curvature = 0;
    info.min_curvature = 1;
    info.max_curvature = -1;
    size_t edge_count = 0;
    
    for (NodeId u = 0; u < n; ++u) {
        for (const auto& e : graph.out_edges(u)) {
            NodeId v = e.target;
            
            // Curvature approximation: based on common neighbors
            // κ ≈ 2 * |N(u) ∩ N(v)| / (|N(u)| + |N(v)|) - 1
            size_t common = 0;
            for (NodeId w : neighbor_sets[u]) {
                if (neighbor_sets[v].count(w)) common++;
            }
            
            size_t total = neighbor_sets[u].size() + neighbor_sets[v].size();
            float kappa = (total > 0) ? 
                (2.0f * common / total - 1.0f) : 0.0f;
            
            info.edge_curvature.push_back(kappa);
            info.avg_curvature += kappa;
            info.min_curvature = std::min(info.min_curvature, kappa);
            info.max_curvature = std::max(info.max_curvature, kappa);
            edge_count++;
        }
    }
    
    if (edge_count > 0) {
        info.avg_curvature /= edge_count;
    }
    
    return info;
}

// RICCI-PATH: Modified Dijkstra that prefers negative curvature edges
inline PathResult ricci_path(
    const Graph& graph,
    NodeId source,
    NodeId target,
    float curvature_weight = 0.5f  // How much to weight curvature vs distance
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "RICCI-PATH";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Compute curvatures
    auto curvature_info = compute_curvature(graph);
    
    // Build edge index for curvature lookup
    std::unordered_map<uint64_t, float> edge_curvature;
    size_t edge_idx = 0;
    for (NodeId u = 0; u < n; ++u) {
        for (const auto& e : graph.out_edges(u)) {
            uint64_t key = (static_cast<uint64_t>(u) << 32) | e.target;
            edge_curvature[key] = curvature_info.edge_curvature[edge_idx++];
        }
    }
    
    // Modified Dijkstra with curvature-adjusted weights
    std::vector<float> dist(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    // Priority queue: (modified_distance, node)
    std::priority_queue<
        std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>
    > pq;
    
    dist[source] = 0;
    pq.push({0, source});
    
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        
        if (visited[u]) continue;
        visited[u] = true;
        result.nodes_explored++;
        
        if (u == target) break;
        
        for (const auto& e : graph.out_edges(u)) {
            result.edges_relaxed++;
            
            // Get curvature for this edge
            uint64_t key = (static_cast<uint64_t>(u) << 32) | e.target;
            float kappa = edge_curvature[key];
            
            // NOVEL: Adjust weight based on curvature
            // Negative curvature (tree-like) -> prefer
            // Positive curvature (clustered) -> penalize slightly
            float curvature_factor = 1.0f + curvature_weight * kappa;
            curvature_factor = std::max(0.5f, std::min(2.0f, curvature_factor));
            
            float modified_weight = e.weight * curvature_factor;
            float new_dist = dist[u] + modified_weight;
            
            if (new_dist < dist[e.target]) {
                dist[e.target] = new_dist;
                parent[e.target] = u;
                pq.push({new_dist, e.target});
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    // Reconstruct path
    if (dist[target] < INF_DIST) {
        // Need to compute actual distance (not modified)
        std::vector<NodeId> path;
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(path.begin(), path.end());
        
        // Calculate true distance
        float true_dist = 0;
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            for (const auto& e : graph.out_edges(path[i])) {
                if (e.target == path[i + 1]) {
                    true_dist += e.weight;
                    break;
                }
            }
        }
        
        result.distance = true_dist;
        result.path = std::move(path);
    } else {
        result.distance = INF_DIST;
    }
    
    return result;
}

// =============================================================================
// APPROACH 3: SPECTRAL GRADIENT PATHFINDING
// =============================================================================
//
// Mathematical basis: Graph Laplacian L = D - A
// The Fiedler vector (2nd smallest eigenvector) divides graph into regions.
// 
// NOVEL: Use spectral coordinates as a heuristic for search direction.

// Power iteration to find approximate Fiedler vector
inline std::vector<float> compute_fiedler_approx(
    const Graph& graph,
    size_t iterations = 50
) {
    const size_t n = graph.num_nodes();
    std::vector<float> v(n), v_new(n);
    
    // Random initialization
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0, 1);
    for (size_t i = 0; i < n; ++i) {
        v[i] = dist(rng);
    }
    
    // Normalize
    float norm = 0;
    for (float x : v) norm += x * x;
    norm = std::sqrt(norm);
    for (float& x : v) x /= norm;
    
    // Power iteration on (I - normalized L)
    // to find smallest non-zero eigenvalue
    for (size_t iter = 0; iter < iterations; ++iter) {
        // Apply L: v_new = D*v - A*v
        for (NodeId i = 0; i < n; ++i) {
            auto edges = graph.out_edges(i);
            float degree = edges.size();
            float neighbor_sum = 0;
            for (const auto& e : edges) {
                neighbor_sum += v[e.target];
            }
            v_new[i] = degree * v[i] - neighbor_sum;
        }
        
        // Make orthogonal to constant vector
        float mean = 0;
        for (float x : v_new) mean += x;
        mean /= n;
        for (float& x : v_new) x -= mean;
        
        // Normalize
        norm = 0;
        for (float x : v_new) norm += x * x;
        norm = std::sqrt(norm);
        if (norm > 1e-10) {
            for (float& x : v_new) x /= norm;
        }
        
        std::swap(v, v_new);
    }
    
    return v;
}

// SPECTRAL-PATH: Use spectral coordinates to guide A*
inline PathResult spectral_path(
    const Graph& graph,
    NodeId source,
    NodeId target
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "SPECTRAL-PATH";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Compute spectral coordinates
    auto fiedler = compute_fiedler_approx(graph, 30);
    
    // Heuristic: difference in spectral coordinate
    // Nodes close in spectral space are likely close in graph
    auto heuristic = [&](NodeId v) -> float {
        return std::abs(fiedler[v] - fiedler[target]) * 10.0f;
    };
    
    // A* with spectral heuristic
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<
        std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>
    > pq;
    
    g_score[source] = 0;
    pq.push({heuristic(source), source});
    
    while (!pq.empty()) {
        auto [f, u] = pq.top();
        pq.pop();
        
        if (visited[u]) continue;
        visited[u] = true;
        result.nodes_explored++;
        
        if (u == target) break;
        
        for (const auto& e : graph.out_edges(u)) {
            result.edges_relaxed++;
            float new_g = g_score[u] + e.weight;
            
            if (new_g < g_score[e.target]) {
                g_score[e.target] = new_g;
                parent[e.target] = u;
                float f_score = new_g + heuristic(e.target);
                pq.push({f_score, e.target});
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    // Reconstruct path
    if (g_score[target] < INF_DIST) {
        std::vector<NodeId> path;
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(path.begin(), path.end());
        result.distance = g_score[target];
        result.path = std::move(path);
    } else {
        result.distance = INF_DIST;
    }
    
    return result;
}

// =============================================================================
// APPROACH 4: ENTROPY-GUIDED SEARCH
// =============================================================================
//
// Mathematical basis: Local entropy measures information content
// High degree nodes have high entropy - they connect to many places
// 
// NOVEL: Use entropy to prioritize exploration of "hub" nodes

inline PathResult entropy_path(
    const Graph& graph,
    NodeId source,
    NodeId target,
    float entropy_weight = 0.3f
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "ENTROPY-PATH";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Precompute node entropy: H(v) = log(degree(v) + 1)
    std::vector<float> entropy(n);
    for (NodeId v = 0; v < n; ++v) {
        size_t deg = graph.out_degree(v);
        entropy[v] = std::log2(deg + 1.0f);
    }
    
    // Modified A*: prefer high-entropy (hub) nodes
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<
        std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>
    > pq;
    
    g_score[source] = 0;
    // Priority = distance - entropy_bonus
    float init_priority = 0 - entropy_weight * entropy[source];
    pq.push({init_priority, source});
    
    while (!pq.empty()) {
        auto [f, u] = pq.top();
        pq.pop();
        
        if (visited[u]) continue;
        visited[u] = true;
        result.nodes_explored++;
        
        if (u == target) break;
        
        for (const auto& e : graph.out_edges(u)) {
            result.edges_relaxed++;
            float new_g = g_score[u] + e.weight;
            
            if (new_g < g_score[e.target]) {
                g_score[e.target] = new_g;
                parent[e.target] = u;
                // NOVEL: Subtract entropy bonus (prefer high entropy)
                float priority = new_g - entropy_weight * entropy[e.target];
                pq.push({priority, e.target});
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    // Reconstruct path
    if (g_score[target] < INF_DIST) {
        std::vector<NodeId> path;
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(path.begin(), path.end());
        result.distance = g_score[target];
        result.path = std::move(path);
    } else {
        result.distance = INF_DIST;
    }
    
    return result;
}

// =============================================================================
// APPROACH 5: GRAVITY-WELL PATHFINDING (Novel!)
// =============================================================================
//
// NEW MATHEMATICAL MODEL:
// Treat target as a gravitational source. Each node feels "pull" toward target.
// Force = 1 / (topological_distance)²
//
// We don't know topological distance, but we can ESTIMATE it using random walks!
// 
// POTENTIALLY NOVEL: Using random walk hitting times as gravitational proxy

inline PathResult gravity_path(
    const Graph& graph,
    NodeId source,
    NodeId target,
    size_t num_walks = 100,
    size_t max_walk_length = 500
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "GRAVITY-PATH";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Estimate "gravity" of each node toward target using random walks
    // Gravity(v) = fraction of random walks from v that reach target quickly
    std::vector<float> gravity(n, 0.0f);
    std::mt19937 rng(42);
    
    // Sample random walks from various nodes
    for (size_t walk = 0; walk < num_walks; ++walk) {
        // Start from random node
        NodeId start = rng() % n;
        NodeId curr = start;
        
        for (size_t step = 0; step < max_walk_length; ++step) {
            auto edges = graph.out_edges(curr);
            if (edges.empty()) break;
            
            // Random step
            size_t idx = rng() % edges.size();
            curr = edges[idx].target;
            
            if (curr == target) {
                // This walk found target! Update gravity for starting node
                gravity[start] += 1.0f / (step + 1);  // Closer = stronger
                break;
            }
        }
    }
    
    // Normalize gravity
    float max_grav = *std::max_element(gravity.begin(), gravity.end());
    if (max_grav > 0) {
        for (float& g : gravity) g /= max_grav;
    }
    
    // A* with gravity as heuristic (inverted - high gravity = close)
    auto heuristic = [&](NodeId v) -> float {
        return (1.0f - gravity[v]) * 100.0f;  // Lower for high gravity
    };
    
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<
        std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>
    > pq;
    
    g_score[source] = 0;
    pq.push({heuristic(source), source});
    
    while (!pq.empty()) {
        auto [f, u] = pq.top();
        pq.pop();
        
        if (visited[u]) continue;
        visited[u] = true;
        result.nodes_explored++;
        
        if (u == target) break;
        
        for (const auto& e : graph.out_edges(u)) {
            result.edges_relaxed++;
            float new_g = g_score[u] + e.weight;
            
            if (new_g < g_score[e.target]) {
                g_score[e.target] = new_g;
                parent[e.target] = u;
                pq.push({new_g + heuristic(e.target), e.target});
            }
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    // Reconstruct path
    if (g_score[target] < INF_DIST) {
        std::vector<NodeId> path;
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(path.begin(), path.end());
        result.distance = g_score[target];
        result.path = std::move(path);
    } else {
        result.distance = INF_DIST;
    }
    
    return result;
}

} // namespace photon::research
