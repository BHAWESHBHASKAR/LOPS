/**
 * PHOTON Search Algorithms
 * 
 * All search strategies in one header for easy comparison.
 */

#pragma once

#include "photon/core.hpp"
#include "photon/graph.hpp"
#include "photon/simd_queue.hpp"

#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>

namespace photon {
namespace search {

// =============================================================================
// Classic Dijkstra (Baseline)
// =============================================================================

inline PathResult dijkstra(
    const Graph& graph,
    NodeId source,
    NodeId target
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "DIJKSTRA";
    
    const size_t n = graph.num_nodes();
    if (source >= n || target >= n) return result;
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    SIMDPriorityQueue pq(n);
    std::vector<NodeId> parent(n, INVALID_NODE);
    
    pq.push(source, 0);
    
    while (!pq.empty()) {
        auto [dist, node] = pq.pop();
        if (node == INVALID_NODE) break;
        
        ++result.nodes_explored;
        
        if (node == target) {
            result.distance = dist;
            break;
        }
        
        // Relax outgoing edges
        for (const auto& edge : graph.out_edges(node)) {
            Distance new_dist = dist + edge.weight;
            if (new_dist < pq.distance(edge.target)) {
                pq.push(edge.target, new_dist);
                parent[edge.target] = node;
                ++result.edges_relaxed;
            }
        }
    }
    
    // Reconstruct path
    if (result.distance != INF_DIST) {
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            result.path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(result.path.begin(), result.path.end());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    return result;
}

// =============================================================================
// Bidirectional Dijkstra with SIMD
// =============================================================================

inline PathResult bidirectional_simd(
    const Graph& graph,
    NodeId source,
    NodeId target
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "BIDIRECTIONAL_SIMD";
    
    const size_t n = graph.num_nodes();
    if (source >= n || target >= n) return result;
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    if (!graph.has_reverse()) {
        // Fall back to unidirectional
        return dijkstra(graph, source, target);
    }
    
    // Forward and backward priority queues
    SIMDPriorityQueue fwd_pq(n), bwd_pq(n);
    std::vector<NodeId> fwd_parent(n, INVALID_NODE), bwd_parent(n, INVALID_NODE);
    std::vector<bool> fwd_settled(n, false), bwd_settled(n, false);
    
    fwd_pq.push(source, 0);
    bwd_pq.push(target, 0);
    
    Distance mu = INF_DIST;  // Best path found
    NodeId meeting_node = INVALID_NODE;
    
    while (!fwd_pq.empty() || !bwd_pq.empty()) {
        // Check stopping criterion
        auto [fwd_min, _f] = fwd_pq.peek();
        auto [bwd_min, _b] = bwd_pq.peek();
        
        if (fwd_min + bwd_min >= mu) {
            break;  // Optimal found!
        }
        
        // Expand direction with smaller minimum (adaptive balancing)
        bool expand_forward = fwd_min <= bwd_min;
        
        if (expand_forward && !fwd_pq.empty()) {
            auto [dist, node] = fwd_pq.pop();
            if (node == INVALID_NODE || fwd_settled[node]) continue;
            
            fwd_settled[node] = true;
            ++result.nodes_explored;
            
            // Check for meeting
            if (bwd_settled[node]) {
                Distance path_len = dist + bwd_pq.distance(node);
                if (path_len < mu) {
                    mu = path_len;
                    meeting_node = node;
                }
            }
            
            // Relax forward edges (SIMD-optimized)
            auto edges = graph.out_edges(node);
            fwd_pq.relax_batch_simd(node, dist, edges);
            
            for (const auto& edge : edges) {
                if (dist + edge.weight < fwd_pq.distance(edge.target)) {
                    fwd_parent[edge.target] = node;
                }
                ++result.edges_relaxed;
            }
        } else if (!bwd_pq.empty()) {
            auto [dist, node] = bwd_pq.pop();
            if (node == INVALID_NODE || bwd_settled[node]) continue;
            
            bwd_settled[node] = true;
            ++result.nodes_explored;
            
            // Check for meeting
            if (fwd_settled[node]) {
                Distance path_len = fwd_pq.distance(node) + dist;
                if (path_len < mu) {
                    mu = path_len;
                    meeting_node = node;
                }
            }
            
            // Relax backward edges (SIMD-optimized)
            auto edges = graph.in_edges(node);
            bwd_pq.relax_batch_simd(node, dist, edges);
            
            for (const auto& edge : edges) {
                if (dist + edge.weight < bwd_pq.distance(edge.target)) {
                    bwd_parent[edge.target] = node;
                }
                ++result.edges_relaxed;
            }
        } else {
            break;
        }
    }
    
    // Reconstruct path
    if (meeting_node != INVALID_NODE && mu != INF_DIST) {
        result.distance = mu;
        
        // Forward path (source -> meeting)
        std::vector<NodeId> fwd_path;
        NodeId curr = meeting_node;
        while (curr != INVALID_NODE) {
            fwd_path.push_back(curr);
            curr = fwd_parent[curr];
        }
        std::reverse(fwd_path.begin(), fwd_path.end());
        
        // Backward path (meeting -> target)
        std::vector<NodeId> bwd_path;
        curr = bwd_parent[meeting_node];
        while (curr != INVALID_NODE) {
            bwd_path.push_back(curr);
            curr = bwd_parent[curr];
        }
        
        // Combine
        result.path = std::move(fwd_path);
        result.path.insert(result.path.end(), bwd_path.begin(), bwd_path.end());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    return result;
}

// =============================================================================
// Delta-Stepping (Parallel)
// =============================================================================

inline PathResult delta_stepping(
    const Graph& graph,
    NodeId source,
    NodeId target,
    Distance delta = 0,  // 0 = auto-tune
    size_t num_threads = 0  // 0 = auto
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "DELTA_STEPPING";
    
    const size_t n = graph.num_nodes();
    if (source >= n || target >= n) return result;
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Auto-tune delta
    if (delta == 0) {
        // Heuristic: delta = average edge weight / log(n)
        float avg_weight = 0;
        size_t count = 0;
        for (size_t i = 0; i < std::min(n, size_t(1000)); ++i) {
            for (const auto& edge : graph.out_edges(i)) {
                avg_weight += edge.weight;
                ++count;
            }
        }
        delta = count > 0 ? avg_weight / count / std::log2(n + 1) : 1.0f;
        delta = std::max(delta, 0.001f);
    }
    
    // Auto-detect threads
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
    }
    
    // Use bucket queue for delta-stepping
    BucketQueue bq(n, delta);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<std::atomic<Distance>> distances(n);
    for (auto& d : distances) d.store(INF_DIST, std::memory_order_relaxed);
    
    distances[source].store(0, std::memory_order_relaxed);
    bq.push(source, 0);
    
    std::atomic<bool> found_target{false};
    std::mutex pq_mutex;
    
    while (!bq.empty() && !found_target) {
        auto bucket_nodes = bq.pop_bucket();
        if (bucket_nodes.empty()) continue;
        
        result.nodes_explored += bucket_nodes.size();
        
        // Light edges (weight < delta) - can be processed in parallel
        std::vector<std::tuple<NodeId, NodeId, Distance>> light_updates;
        
        // Process bucket in parallel
        #pragma omp parallel for if(bucket_nodes.size() > 64) num_threads(num_threads)
        for (size_t i = 0; i < bucket_nodes.size(); ++i) {
            NodeId node = bucket_nodes[i];
            Distance dist = distances[node].load(std::memory_order_relaxed);
            
            if (node == target) {
                found_target = true;
            }
            
            // Relax outgoing edges
            for (const auto& edge : graph.out_edges(node)) {
                Distance new_dist = dist + edge.weight;
                
                // Atomic compare-and-swap for thread safety
                Distance old_dist = distances[edge.target].load(std::memory_order_relaxed);
                while (new_dist < old_dist) {
                    if (distances[edge.target].compare_exchange_weak(
                        old_dist, new_dist, std::memory_order_release, std::memory_order_relaxed
                    )) {
                        #pragma omp critical
                        {
                            bq.push(edge.target, new_dist);
                            parent[edge.target] = node;
                        }
                        break;
                    }
                }
            }
        }
    }
    
    result.distance = distances[target].load();
    
    // Reconstruct path
    if (result.distance != INF_DIST) {
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            result.path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(result.path.begin(), result.path.end());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    return result;
}

// =============================================================================
// A* with Euclidean Heuristic
// =============================================================================

inline PathResult a_star(
    const Graph& graph,
    NodeId source,
    NodeId target,
    std::function<Distance(NodeId, NodeId)> heuristic = nullptr
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "A_STAR";
    
    const size_t n = graph.num_nodes();
    if (source >= n || target >= n) return result;
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Default to Euclidean if coordinates available
    if (!heuristic && graph.has_coordinates()) {
        heuristic = [&graph, target](NodeId from, NodeId) {
            return graph.euclidean_distance(from, target);
        };
    }
    
    // Fall back to Dijkstra if no heuristic
    if (!heuristic) {
        heuristic = [](NodeId, NodeId) { return Distance(0); };
    }
    
    // Priority queue: stores (f-score, node)
    SIMDPriorityQueue pq(n);
    std::vector<Distance> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> closed(n, false);
    
    g_score[source] = 0;
    pq.push(source, heuristic(source, target));
    
    while (!pq.empty()) {
        auto [f, node] = pq.pop();
        if (node == INVALID_NODE) break;
        
        if (closed[node]) continue;
        closed[node] = true;
        ++result.nodes_explored;
        
        if (node == target) {
            result.distance = g_score[target];
            break;
        }
        
        for (const auto& edge : graph.out_edges(node)) {
            Distance tentative_g = g_score[node] + edge.weight;
            
            if (tentative_g < g_score[edge.target]) {
                g_score[edge.target] = tentative_g;
                parent[edge.target] = node;
                Distance f_score = tentative_g + heuristic(edge.target, target);
                pq.push(edge.target, f_score);
                ++result.edges_relaxed;
            }
        }
    }
    
    // Reconstruct path
    if (result.distance != INF_DIST) {
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            result.path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(result.path.begin(), result.path.end());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    return result;
}

// =============================================================================
// Parallel Wavefront (Lock-Free)
// =============================================================================

inline PathResult parallel_wavefront(
    const Graph& graph,
    NodeId source,
    NodeId target,
    size_t num_threads = 0
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "PARALLEL_WAVE";
    
    const size_t n = graph.num_nodes();
    if (source >= n || target >= n) return result;
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
    }
    
    // Lock-free distance array
    std::vector<std::atomic<Distance>> distances(n);
    for (auto& d : distances) d.store(INF_DIST, std::memory_order_relaxed);
    
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::mutex parent_mutex;
    
    // Current and next frontiers
    std::vector<NodeId> current_frontier, next_frontier;
    std::mutex frontier_mutex;
    
    distances[source].store(0, std::memory_order_relaxed);
    current_frontier.push_back(source);
    
    std::atomic<bool> target_found{false};
    
    while (!current_frontier.empty() && !target_found) {
        result.nodes_explored += current_frontier.size();
        
        // Process frontier in parallel
        #pragma omp parallel for num_threads(num_threads) schedule(dynamic, 64)
        for (size_t i = 0; i < current_frontier.size(); ++i) {
            NodeId node = current_frontier[i];
            Distance dist = distances[node].load(std::memory_order_relaxed);
            
            if (node == target) {
                target_found = true;
            }
            
            for (const auto& edge : graph.out_edges(node)) {
                Distance new_dist = dist + edge.weight;
                
                Distance old_dist = distances[edge.target].load(std::memory_order_relaxed);
                while (new_dist < old_dist) {
                    if (distances[edge.target].compare_exchange_weak(
                        old_dist, new_dist,
                        std::memory_order_release,
                        std::memory_order_relaxed
                    )) {
                        {
                            std::lock_guard<std::mutex> lock(parent_mutex);
                            parent[edge.target] = node;
                        }
                        {
                            std::lock_guard<std::mutex> lock(frontier_mutex);
                            next_frontier.push_back(edge.target);
                        }
                        break;
                    }
                }
            }
        }
        
        // Swap frontiers
        current_frontier.clear();
        std::swap(current_frontier, next_frontier);
        
        // Remove duplicates (simple dedup)
        std::sort(current_frontier.begin(), current_frontier.end());
        auto last = std::unique(current_frontier.begin(), current_frontier.end());
        current_frontier.erase(last, current_frontier.end());
    }
    
    result.distance = distances[target].load();
    
    // Reconstruct path
    if (result.distance != INF_DIST) {
        NodeId curr = target;
        while (curr != INVALID_NODE) {
            result.path.push_back(curr);
            curr = parent[curr];
        }
        std::reverse(result.path.begin(), result.path.end());
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(end_time - start_time).count();
    
    return result;
}

} // namespace search
} // namespace photon
