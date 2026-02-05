/**
 * PHOTON Engine - Main Entry Point
 * 
 * The unified PHOTON interface that automatically selects the best strategy.
 */

#pragma once

#include "photon/core.hpp"
#include "photon/graph.hpp"
#include "photon/simd_queue.hpp"
#include "photon/search.hpp"

#include <memory>
#include <unordered_map>
#include <shared_mutex>

namespace photon {

// =============================================================================
// Query Cache
// =============================================================================

class QueryCache {
public:
    explicit QueryCache(size_t max_size = 10000) : max_size_(max_size) {}
    
    void put(NodeId source, NodeId target, const PathResult& result) {
        std::unique_lock lock(mutex_);
        
        uint64_t key = make_key(source, target);
        cache_[key] = result;
        
        // Simple eviction: clear half when full
        if (cache_.size() > max_size_) {
            auto it = cache_.begin();
            std::advance(it, max_size_ / 2);
            cache_.erase(cache_.begin(), it);
        }
    }
    
    std::optional<PathResult> get(NodeId source, NodeId target) const {
        std::shared_lock lock(mutex_);
        
        uint64_t key = make_key(source, target);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    void clear() {
        std::unique_lock lock(mutex_);
        cache_.clear();
    }
    
    [[nodiscard]] size_t size() const {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }

private:
    static uint64_t make_key(NodeId a, NodeId b) {
        return (static_cast<uint64_t>(a) << 32) | b;
    }
    
    mutable std::shared_mutex mutex_;
    std::unordered_map<uint64_t, PathResult> cache_;
    size_t max_size_;
};

// =============================================================================
// Strategy Selector
// =============================================================================

class StrategySelector {
public:
    explicit StrategySelector(const PhotonConfig& config) : config_(config) {}
    
    [[nodiscard]] Strategy select(const Graph& graph, NodeId source, NodeId target) const {
        if (config_.preferred_strategy != Strategy::AUTO) {
            return config_.preferred_strategy;
        }
        
        const auto& stats = graph.stats();
        
        // Decision tree based on graph characteristics
        
        // 1. Very small graph: just use Dijkstra
        if (stats.num_nodes < 100) {
            return Strategy::DIJKSTRA;
        }
        
        // 2. Unweighted graph: prefer parallel wavefront or bidirectional BFS
        if (!stats.is_weighted && config_.num_threads > 1) {
            return Strategy::PARALLEL_WAVE;
        }
        
        // 3. Has coordinates: use A*
        if (stats.has_coordinates) {
            return Strategy::A_STAR;
        }
        
        // 4. Small-world (social network): bidirectional wins big
        if (stats.is_small_world()) {
            return Strategy::BIDIRECTIONAL_SIMD;
        }
        
        // 5. Grid-like: A* with zero heuristic or delta-stepping
        if (stats.is_grid_like() && config_.num_threads > 1) {
            return Strategy::DELTA_STEPPING;
        }
        
        // 6. Large sparse graph: bidirectional
        if (stats.is_sparse() && stats.num_nodes > 1000) {
            return Strategy::BIDIRECTIONAL_SIMD;
        }
        
        // 7. Large graph with many threads: parallel
        if (stats.num_nodes > 10000 && config_.num_threads > 4) {
            return Strategy::DELTA_STEPPING;
        }
        
        // 8. Default: bidirectional SIMD
        return Strategy::BIDIRECTIONAL_SIMD;
    }

private:
    PhotonConfig config_;
};

// =============================================================================
// PHOTON Engine
// =============================================================================

class PhotonEngine {
public:
    explicit PhotonEngine(PhotonConfig config = {})
        : config_(std::move(config))
        , cache_(config_.query_cache_size)
        , selector_(config_)
    {
        // Auto-detect thread count
        if (config_.num_threads == 0) {
            config_.num_threads = std::thread::hardware_concurrency();
        }
    }
    
    // Load graph (takes ownership)
    void load_graph(Graph graph) {
        graph_ = std::make_unique<Graph>(std::move(graph));
        cache_.clear();
        query_count_ = 0;
    }
    
    // Check if graph is loaded
    [[nodiscard]] bool ready() const noexcept {
        return graph_ != nullptr;
    }
    
    // Get graph reference
    [[nodiscard]] const Graph& graph() const {
        if (!graph_) throw std::runtime_error("No graph loaded");
        return *graph_;
    }
    
    // ==========================================================================
    // Main Query Interface
    // ==========================================================================
    
    [[nodiscard]] PathResult find_path(NodeId source, NodeId target) {
        if (!graph_) {
            PathResult r;
            r.strategy_used = "ERROR: No graph loaded";
            return r;
        }
        
        ++query_count_;
        
        // Check cache first
        if (config_.enable_query_cache) {
            if (auto cached = cache_.get(source, target)) {
                cached->strategy_used = "CACHED";
                return *cached;
            }
        }
        
        // Select strategy
        Strategy strategy = selector_.select(*graph_, source, target);
        
        // Execute search
        PathResult result;
        
        switch (strategy) {
            case Strategy::DIJKSTRA:
                result = search::dijkstra(*graph_, source, target);
                break;
                
            case Strategy::BIDIRECTIONAL:
            case Strategy::BIDIRECTIONAL_SIMD:
                result = search::bidirectional_simd(*graph_, source, target);
                break;
                
            case Strategy::DELTA_STEPPING:
                result = search::delta_stepping(*graph_, source, target, 
                    config_.delta, config_.num_threads);
                break;
                
            case Strategy::A_STAR:
                result = search::a_star(*graph_, source, target);
                break;
                
            case Strategy::PARALLEL_WAVE:
                result = search::parallel_wavefront(*graph_, source, target, 
                    config_.num_threads);
                break;
                
            default:
                result = search::bidirectional_simd(*graph_, source, target);
                break;
        }
        
        // Verify if requested
        if (config_.verify_results && result.found()) {
            auto verified = search::dijkstra(*graph_, source, target);
            if (std::abs(result.distance - verified.distance) > 1e-6) {
                result.strategy_used = "ERROR: Verification failed";
            }
        }
        
        // Cache result
        if (config_.enable_query_cache && result.found()) {
            cache_.put(source, target, result);
        }
        
        return result;
    }
    
    // ==========================================================================
    // Batch Query
    // ==========================================================================
    
    [[nodiscard]] std::vector<PathResult> find_paths_batch(
        std::span<const std::pair<NodeId, NodeId>> queries
    ) {
        std::vector<PathResult> results(queries.size());
        
        #pragma omp parallel for schedule(dynamic)
        for (size_t i = 0; i < queries.size(); ++i) {
            results[i] = find_path(queries[i].first, queries[i].second);
        }
        
        return results;
    }
    
    // ==========================================================================
    // Statistics
    // ==========================================================================
    
    [[nodiscard]] size_t query_count() const noexcept { return query_count_; }
    [[nodiscard]] size_t cache_size() const { return cache_.size(); }
    [[nodiscard]] const PhotonConfig& config() const noexcept { return config_; }

private:
    PhotonConfig config_;
    std::unique_ptr<Graph> graph_;
    QueryCache cache_;
    StrategySelector selector_;
    std::atomic<size_t> query_count_{0};
};

// =============================================================================
// Convenience Functions
// =============================================================================

inline PathResult shortest_path(
    const Graph& graph,
    NodeId source,
    NodeId target,
    Strategy strategy = Strategy::AUTO
) {
    PhotonConfig config;
    config.preferred_strategy = strategy;
    
    switch (strategy) {
        case Strategy::DIJKSTRA:
            return search::dijkstra(graph, source, target);
        case Strategy::BIDIRECTIONAL:
        case Strategy::BIDIRECTIONAL_SIMD:
            return search::bidirectional_simd(graph, source, target);
        case Strategy::DELTA_STEPPING:
            return search::delta_stepping(graph, source, target);
        case Strategy::A_STAR:
            return search::a_star(graph, source, target);
        case Strategy::PARALLEL_WAVE:
            return search::parallel_wavefront(graph, source, target);
        default:
            // AUTO: use strategy selector
            StrategySelector selector(config);
            Strategy selected = selector.select(graph, source, target);
            return shortest_path(graph, source, target, selected);
    }
}

} // namespace photon
