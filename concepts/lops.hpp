/**
 * LOPS: Lipschitz-Optimized Potential Search
 *
 * A math-driven shortest path algorithm that builds multiple global
 * 1-Lipschitz potentials and uses their maximum difference as an
 * admissible and consistent A* heuristic.
 *
 * Core idea:
 *   Build potentials via multi-source Dijkstra with random anchor labels:
 *     phi(v) = min_i (label_i + dist(anchor_i, v))
 *   Each phi is 1-Lipschitz, so:
 *     |phi(u) - phi(v)| <= dist(u, v)
 *   Heuristic:
 *     h(v) = max_i |phi_i(v) - phi_i(t)|
 */

#pragma once

#include "photon/core.hpp"
#include "photon/graph.hpp"

#include <vector>
#include <queue>
#include <random>
#include <cmath>
#include <algorithm>
#include <chrono>

namespace photon {
namespace lops {

struct LOPSParams {
    uint32_t num_potentials = 8;  // 0 = auto
    uint32_t num_anchors = 16;     // 0 = auto
    uint32_t num_landmarks = 0;    // 0 = auto (ALT-style landmarks)
    uint32_t seed = 42;
    float label_range = 0.0f;  // 0 = auto
    bool degree_weighted = true;
    bool use_landmarks = true;
    bool landmarks_farthest = true;
    bool symmetric_labels = true;
    bool use_bidirectional = true;
    bool bidir_for_exact = false;
    bool bidir_for_weighted = true;
    bool use_upper_bound = false;
    uint32_t num_upper_bound_roots = 4;
};

class LOPS {
public:
    void preprocess(const Graph& graph, const LOPSParams& params = {}) {
        auto start = std::chrono::high_resolution_clock::now();

        graph_ = &graph;
        params_ = params;
        n_ = graph.num_nodes();
        preprocessed_ = false;

        if (n_ == 0) {
            preprocessed_ = true;
            return;
        }

        build_undirected_adjacency();
        compute_anchor_weights();
        compute_degree_cv();
        build_reverse_adjacency();

        // Auto-tune parameters
        if (params_.num_potentials == 0) {
            uint32_t base = static_cast<uint32_t>(std::clamp<size_t>(
                static_cast<size_t>(std::log2(n_)) + 2, 6, 18));
            if (cv_ > 1.0f) base = std::max<uint32_t>(6, base - 4);
            else if (cv_ > 0.6f) base = std::max<uint32_t>(6, base - 2);
            else if (cv_ < 0.2f) base = std::min<uint32_t>(24, base + 6);
            else if (cv_ < 0.4f) base = std::min<uint32_t>(20, base + 4);
            params_.num_potentials = base;
        }
        if (params_.num_anchors == 0) {
            uint32_t base = static_cast<uint32_t>(std::clamp<size_t>(
                static_cast<size_t>(std::sqrt(n_)), 12, 64));
            if (cv_ > 1.0f) base = std::max<uint32_t>(12, base - 16);
            else if (cv_ > 0.6f) base = std::max<uint32_t>(12, base - 8);
            else if (cv_ < 0.2f) base = std::min<uint32_t>(96, base + 16);
            else if (cv_ < 0.4f) base = std::min<uint32_t>(80, base + 8);
            params_.num_anchors = base;
        }
        if (params_.label_range <= 0.0f) {
            float scale = std::max(4.0f, static_cast<float>(std::log2(n_)) + 1.0f);
            params_.label_range = avg_edge_weight_ * scale * (1.0f + 0.5f * cv_);
            if (params_.label_range <= 0.0f) params_.label_range = 1.0f;
        }
        if (params_.num_landmarks == 0) {
            uint32_t base = static_cast<uint32_t>(std::clamp<size_t>(
                static_cast<size_t>(std::log2(n_)) + 1, 4, 16));
            if (cv_ > 1.0f) base = 4;
            else if (cv_ > 0.6f) base = std::min<uint32_t>(6, base);
            else if (cv_ < 0.2f) base = std::min<uint32_t>(24, base + 6);
            else if (cv_ < 0.4f) base = std::min<uint32_t>(20, base + 4);
            params_.num_landmarks = base;
        }

        potentials_.clear();
        potentials_.reserve(params_.num_potentials);

        std::mt19937 rng(params_.seed);

        landmarks_.clear();
        landmark_dists_.clear();
        if (params_.use_landmarks && params_.num_landmarks > 0) {
            build_landmarks(rng);
        }

        for (uint32_t i = 0; i < params_.num_potentials; ++i) {
            auto anchors = sample_anchors(rng, params_.num_anchors);
            float range = params_.label_range * (1.0f + 0.15f * static_cast<float>(i));
            auto labels = sample_labels(rng, anchors.size(), range, params_.symmetric_labels);
            potentials_.push_back(multi_source_dijkstra(anchors, labels));
        }

        if (params_.use_upper_bound && params_.num_upper_bound_roots > 0) {
            build_upper_bound_tables(rng);
        }

        auto end = std::chrono::high_resolution_clock::now();
        preprocessing_time_us_ = std::chrono::duration<double, std::micro>(
            end - start).count();

        preprocessed_ = true;
    }

    PathResult query(NodeId source, NodeId target, float heuristic_weight = 1.0f) const {
        return run_search(source, target, heuristic_weight, INF_DIST, "LIPS");
    }

    PathResult query_exact(NodeId source, NodeId target) const {
        return run_search(source, target, 1.0f, INF_DIST, "LIPS-Exact");
    }

    PathResult query_approx(NodeId source, NodeId target, float heuristic_weight = 1.3f) const {
        return run_search(source, target, heuristic_weight, INF_DIST, "LIPS-Weighted");
    }

    PathResult query_hybrid(NodeId source, NodeId target, float approx_weight = 1.3f) const {
        auto approx = run_search(source, target, approx_weight, INF_DIST, "LIPS-Weighted");
        if (!approx.found()) {
            return approx;
        }
        auto exact = run_search(source, target, 1.0f, approx.distance, "LIPS-Hybrid");
        return exact;
    }

private:
    PathResult run_search(
        NodeId source,
        NodeId target,
        float heuristic_weight,
        float upper_bound,
        const char* strategy_name
    ) const {
        const bool use_bidir = params_.use_bidirectional &&
            ((heuristic_weight == 1.0f && params_.bidir_for_exact) ||
             (heuristic_weight != 1.0f && params_.bidir_for_weighted));
        if (use_bidir) {
            return run_search_bidir(source, target, heuristic_weight, upper_bound, strategy_name);
        }
        return run_search_unidir(source, target, heuristic_weight, upper_bound, strategy_name);
    }

    PathResult run_search_unidir(
        NodeId source,
        NodeId target,
        float heuristic_weight,
        float upper_bound,
        const char* strategy_name
    ) const {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        PathResult result;
        result.strategy_used = strategy_name;
        
        if (!graph_ || !preprocessed_) {
            result.strategy_used = "ERROR: LOPS not preprocessed";
            return result;
        }

        if (source == target) {
            result.distance = 0;
            result.path = {source};
            return result;
        }

        std::vector<float> g_score(n_, INF_DIST);
        std::vector<NodeId> parent(n_, INVALID_NODE);
        std::vector<bool> closed(n_, false);

        struct PQItem {
            float f;
            float g;
            NodeId node;
        };
        struct PQCmp {
            bool operator()(const PQItem& a, const PQItem& b) const {
                return a.f > b.f;
            }
        };
        std::priority_queue<PQItem, std::vector<PQItem>, PQCmp> pq;

        g_score[source] = 0.0f;
        const float ub = (heuristic_weight == 1.0f) ? upper_bound : INF_DIST;
        float h0 = heuristic(source, target);
        pq.push({heuristic_weight * h0, 0.0f, source});

        while (!pq.empty()) {
            auto [f, g, u] = pq.top();
            pq.pop();

            if (g != g_score[u]) {
                continue;  // Skip stale entry
            }
            if (closed[u]) continue;
            closed[u] = true;
            ++result.nodes_explored;
            
            if (u == target) break;

            for (const auto& e : graph_->out_edges(u)) {
                ++result.edges_relaxed;
                float ng = g_score[u] + e.weight;
                float h = heuristic(e.target, target);
                float nf = ng + heuristic_weight * h;
                if (ub < INF_DIST && (ng + h) > ub) {
                    continue;
                }
                if (ng < g_score[e.target]) {
                    g_score[e.target] = ng;
                    parent[e.target] = u;
                    pq.push({nf, ng, e.target});
                }
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.time_microseconds = std::chrono::duration<double, std::micro>(
            end_time - start_time).count();

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

    PathResult run_search_bidir(
        NodeId source,
        NodeId target,
        float heuristic_weight,
        float upper_bound,
        const char* strategy_name
    ) const {
        auto start_time = std::chrono::high_resolution_clock::now();

        PathResult result;
        result.strategy_used = strategy_name;

        if (!graph_ || !preprocessed_) {
            result.strategy_used = "ERROR: LOPS not preprocessed";
            return result;
        }

        if (source == target) {
            result.distance = 0;
            result.path = {source};
            return result;
        }

        std::vector<float> g_f(n_, INF_DIST);
        std::vector<float> g_b(n_, INF_DIST);
        std::vector<NodeId> parent_f(n_, INVALID_NODE);
        std::vector<NodeId> parent_b(n_, INVALID_NODE);
        std::vector<bool> closed_f(n_, false);
        std::vector<bool> closed_b(n_, false);

        struct OpenItem {
            float f;
            float g;
            NodeId node;
        };
        struct OpenCmp {
            bool operator()(const OpenItem& a, const OpenItem& b) const {
                return a.f > b.f;
            }
        };
        struct LbItem {
            float lb;
            float g;
            NodeId node;
        };
        struct LbCmp {
            bool operator()(const LbItem& a, const LbItem& b) const {
                return a.lb > b.lb;
            }
        };

        std::priority_queue<OpenItem, std::vector<OpenItem>, OpenCmp> open_f;
        std::priority_queue<OpenItem, std::vector<OpenItem>, OpenCmp> open_b;
        std::priority_queue<LbItem, std::vector<LbItem>, LbCmp> lb_f;
        std::priority_queue<LbItem, std::vector<LbItem>, LbCmp> lb_b;

        auto h_to_t = [&](NodeId v) { return heuristic(v, target); };
        auto h_to_s = [&](NodeId v) { return heuristic(v, source); };

        g_f[source] = 0.0f;
        g_b[target] = 0.0f;

        float hfs = h_to_t(source);
        float hbt = h_to_s(target);
        open_f.push({hfs * heuristic_weight, 0.0f, source});
        lb_f.push({hfs, 0.0f, source});
        open_b.push({hbt * heuristic_weight, 0.0f, target});
        lb_b.push({hbt, 0.0f, target});

        float best = (upper_bound < INF_DIST) ? upper_bound : INF_DIST;
        NodeId meet = INVALID_NODE;

        auto clean_open = [](auto& pq, const auto& g, const auto& closed) -> bool {
            while (!pq.empty()) {
                const auto& top = pq.top();
                if (top.g != g[top.node] || closed[top.node]) {
                    pq.pop();
                    continue;
                }
                return true;
            }
            return false;
        };

        auto clean_lb = [](auto& pq, const auto& g, const auto& closed) -> float {
            while (!pq.empty()) {
                const auto& top = pq.top();
                if (top.g != g[top.node] || closed[top.node]) {
                    pq.pop();
                    continue;
                }
                return top.lb;
            }
            return INF_DIST;
        };

        while (true) {
            bool has_f = clean_open(open_f, g_f, closed_f);
            bool has_b = clean_open(open_b, g_b, closed_b);
            if (!has_f && !has_b) break;

            float min_lb_f = clean_lb(lb_f, g_f, closed_f);
            float min_lb_b = clean_lb(lb_b, g_b, closed_b);
            if (best < INF_DIST && (min_lb_f + min_lb_b) >= best) {
                break;
            }

            bool expand_forward = false;
            if (!has_b) expand_forward = true;
            else if (!has_f) expand_forward = false;
            else {
                expand_forward = open_f.top().f <= open_b.top().f;
            }

            if (expand_forward) {
                auto item = open_f.top();
                open_f.pop();
                NodeId u = item.node;
                if (closed_f[u]) continue;
                closed_f[u] = true;
                ++result.nodes_explored;

                if (u == target) {
                    best = g_f[u];
                    meet = u;
                    break;
                }

                for (const auto& e : graph_->out_edges(u)) {
                    ++result.edges_relaxed;
                    float ng = g_f[u] + e.weight;
                    float h = h_to_t(e.target);
                    if (upper_bound < INF_DIST && (ng + h) > upper_bound) {
                        continue;
                    }
                    if (ng < g_f[e.target]) {
                        g_f[e.target] = ng;
                        parent_f[e.target] = u;
                        open_f.push({ng + heuristic_weight * h, ng, e.target});
                        lb_f.push({ng + h, ng, e.target});
                        if (g_b[e.target] < INF_DIST) {
                            float cand = ng + g_b[e.target];
                            if (cand < best) {
                                best = cand;
                                meet = e.target;
                            }
                        }
                    }
                }
            } else {
                auto item = open_b.top();
                open_b.pop();
                NodeId u = item.node;
                if (closed_b[u]) continue;
                closed_b[u] = true;
                ++result.nodes_explored;

                if (u == source) {
                    best = g_b[u];
                    meet = u;
                    break;
                }

                for (const auto& e : reverse_[u]) {
                    ++result.edges_relaxed;
                    float ng = g_b[u] + e.weight;
                    float h = h_to_s(e.target);
                    if (upper_bound < INF_DIST && (ng + h) > upper_bound) {
                        continue;
                    }
                    if (ng < g_b[e.target]) {
                        g_b[e.target] = ng;
                        parent_b[e.target] = u;
                        open_b.push({ng + heuristic_weight * h, ng, e.target});
                        lb_b.push({ng + h, ng, e.target});
                        if (g_f[e.target] < INF_DIST) {
                            float cand = ng + g_f[e.target];
                            if (cand < best) {
                                best = cand;
                                meet = e.target;
                            }
                        }
                    }
                }
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        result.time_microseconds = std::chrono::duration<double, std::micro>(
            end_time - start_time).count();

        if (best < INF_DIST && meet != INVALID_NODE) {
            std::vector<NodeId> path_f;
            NodeId curr = meet;
            while (curr != INVALID_NODE) {
                path_f.push_back(curr);
                curr = parent_f[curr];
            }
            std::reverse(path_f.begin(), path_f.end());

            std::vector<NodeId> path_b;
            curr = parent_b[meet];
            while (curr != INVALID_NODE) {
                path_b.push_back(curr);
                curr = parent_b[curr];
            }

            path_f.insert(path_f.end(), path_b.begin(), path_b.end());
            result.distance = best;
            result.path = std::move(path_f);
        } else {
            result.distance = INF_DIST;
        }

        return result;
    }

public:
    [[nodiscard]] bool ready() const noexcept { return preprocessed_; }
    [[nodiscard]] double get_preprocessing_time_us() const { return preprocessing_time_us_; }
    [[nodiscard]] const LOPSParams& params() const noexcept { return params_; }
    [[nodiscard]] float heuristic_lips(NodeId v, NodeId t) const { return heuristic_lips_component(v, t); }
    [[nodiscard]] float heuristic_alt(NodeId v, NodeId t) const { return heuristic_alt_component(v, t); }
    [[nodiscard]] float heuristic_combined(NodeId v, NodeId t) const { return heuristic(v, t); }

private:
    const Graph* graph_ = nullptr;
    size_t n_ = 0;
    bool preprocessed_ = false;
    double preprocessing_time_us_ = 0.0;
    float avg_edge_weight_ = 1.0f;
    float cv_ = 0.0f;
    LOPSParams params_;

    std::vector<std::vector<Edge>> undirected_;
    std::vector<std::vector<Edge>> reverse_;
    std::vector<float> anchor_weights_;
    std::vector<std::vector<float>> potentials_;
    std::vector<NodeId> ub_roots_;
    std::vector<std::vector<float>> dist_from_roots_;
    std::vector<std::vector<float>> dist_to_roots_;
    std::vector<NodeId> landmarks_;
    std::vector<std::vector<float>> landmark_dists_;

    void build_undirected_adjacency() {
        undirected_.clear();
        undirected_.resize(n_);

        double sum_w = 0.0;
        size_t count_w = 0;

        for (NodeId u = 0; u < n_; ++u) {
            for (const auto& e : graph_->out_edges(u)) {
                undirected_[u].push_back(e);
                undirected_[e.target].push_back(Edge{u, e.weight});
                sum_w += e.weight;
                ++count_w;
            }
        }

        avg_edge_weight_ = count_w > 0 ? static_cast<float>(sum_w / count_w) : 1.0f;
    }

    void compute_anchor_weights() {
        anchor_weights_.assign(n_, 1.0f);
        if (!params_.degree_weighted) return;

        for (NodeId v = 0; v < n_; ++v) {
            float deg = static_cast<float>(undirected_[v].size());
            float w = std::log2(deg + 1.0f) + 1.0f;
            anchor_weights_[v] = w * w;
        }
    }

    void compute_degree_cv() {
        if (n_ == 0) {
            cv_ = 0.0f;
            return;
        }
        double sum = 0.0, sum_sq = 0.0;
        for (NodeId v = 0; v < n_; ++v) {
            double d = static_cast<double>(undirected_[v].size());
            sum += d;
            sum_sq += d * d;
        }
        double mean = sum / static_cast<double>(n_);
        double variance = (sum_sq / static_cast<double>(n_)) - (mean * mean);
        if (variance < 0.0) variance = 0.0;
        double stddev = std::sqrt(variance);
        cv_ = static_cast<float>(stddev / (mean + 1e-6));
    }

    void build_reverse_adjacency() {
        reverse_.clear();
        reverse_.resize(n_);
        for (NodeId u = 0; u < n_; ++u) {
            for (const auto& e : graph_->out_edges(u)) {
                reverse_[e.target].push_back(Edge{u, e.weight});
            }
        }
    }

    std::vector<NodeId> sample_anchors(std::mt19937& rng, uint32_t k) const {
        if (k >= n_) {
            std::vector<NodeId> all;
            all.reserve(n_);
            for (NodeId v = 0; v < n_; ++v) all.push_back(v);
            return all;
        }

        std::vector<NodeId> anchors;
        anchors.reserve(k);
        std::vector<uint8_t> used(n_, 0);

        if (params_.degree_weighted) {
            std::discrete_distribution<size_t> dist(anchor_weights_.begin(), anchor_weights_.end());
            while (anchors.size() < k) {
                NodeId v = static_cast<NodeId>(dist(rng));
                if (!used[v]) {
                    used[v] = 1;
                    anchors.push_back(v);
                }
            }
        } else {
            std::uniform_int_distribution<NodeId> dist(0, static_cast<NodeId>(n_ - 1));
            while (anchors.size() < k) {
                NodeId v = dist(rng);
                if (!used[v]) {
                    used[v] = 1;
                    anchors.push_back(v);
                }
            }
        }

        return anchors;
    }

    std::vector<NodeId> sample_roots(std::mt19937& rng, uint32_t k) const {
        if (k >= n_) {
            std::vector<NodeId> all;
            all.reserve(n_);
            for (NodeId v = 0; v < n_; ++v) all.push_back(v);
            return all;
        }

        std::vector<NodeId> roots;
        roots.reserve(k);
        std::vector<uint8_t> used(n_, 0);

        if (params_.degree_weighted) {
            std::discrete_distribution<size_t> dist(anchor_weights_.begin(), anchor_weights_.end());
            while (roots.size() < k) {
                NodeId v = static_cast<NodeId>(dist(rng));
                if (!used[v]) {
                    used[v] = 1;
                    roots.push_back(v);
                }
            }
        } else {
            std::uniform_int_distribution<NodeId> dist(0, static_cast<NodeId>(n_ - 1));
            while (roots.size() < k) {
                NodeId v = dist(rng);
                if (!used[v]) {
                    used[v] = 1;
                    roots.push_back(v);
                }
            }
        }

        return roots;
    }

    NodeId pick_seed_node(std::mt19937& rng) const {
        if (n_ == 0) return INVALID_NODE;
        if (params_.degree_weighted) {
            NodeId best = 0;
            size_t best_deg = undirected_[0].size();
            for (NodeId v = 1; v < n_; ++v) {
                if (undirected_[v].size() > best_deg) {
                    best_deg = undirected_[v].size();
                    best = v;
                }
            }
            return best;
        }
        std::uniform_int_distribution<NodeId> dist(0, static_cast<NodeId>(n_ - 1));
        return dist(rng);
    }

    static std::vector<float> sample_labels(
        std::mt19937& rng,
        size_t k,
        float label_range,
        bool symmetric_labels
    ) {
        std::vector<float> labels(k, 0.0f);
        if (label_range <= 0.0f) return labels;
        std::uniform_real_distribution<float> dist(
            symmetric_labels ? -label_range : 0.0f,
            label_range);
        for (size_t i = 0; i < k; ++i) labels[i] = dist(rng);
        return labels;
    }

    std::vector<float> undirected_dijkstra(NodeId source) const {
        std::vector<float> dist(n_, INF_DIST);
        using PQItem = std::pair<float, NodeId>;
        std::priority_queue<PQItem, std::vector<PQItem>, std::greater<>> pq;

        dist[source] = 0.0f;
        pq.push({0.0f, source});

        while (!pq.empty()) {
            auto [du, u] = pq.top();
            pq.pop();
            if (du != dist[u]) continue;

            for (const auto& e : undirected_[u]) {
                float nd = du + e.weight;
                if (nd < dist[e.target]) {
                    dist[e.target] = nd;
                    pq.push({nd, e.target});
                }
            }
        }

        return dist;
    }

    std::vector<float> multi_source_dijkstra(
        const std::vector<NodeId>& sources,
        const std::vector<float>& labels
    ) const {
        std::vector<float> dist(n_, INF_DIST);
        using PQItem = std::pair<float, NodeId>;
        std::priority_queue<PQItem, std::vector<PQItem>, std::greater<>> pq;

        for (size_t i = 0; i < sources.size(); ++i) {
            NodeId s = sources[i];
            float d = labels[i];
            if (d < dist[s]) {
                dist[s] = d;
                pq.push({d, s});
            }
        }

        while (!pq.empty()) {
            auto [du, u] = pq.top();
            pq.pop();
            if (du != dist[u]) continue;

            for (const auto& e : undirected_[u]) {
                float nd = du + e.weight;
                if (nd < dist[e.target]) {
                    dist[e.target] = nd;
                    pq.push({nd, e.target});
                }
            }
        }

        return dist;
    }

    std::vector<float> dijkstra_forward(NodeId source) const {
        std::vector<float> dist(n_, INF_DIST);
        using PQItem = std::pair<float, NodeId>;
        std::priority_queue<PQItem, std::vector<PQItem>, std::greater<>> pq;

        dist[source] = 0.0f;
        pq.push({0.0f, source});

        while (!pq.empty()) {
            auto [du, u] = pq.top();
            pq.pop();
            if (du != dist[u]) continue;

            for (const auto& e : graph_->out_edges(u)) {
                float nd = du + e.weight;
                if (nd < dist[e.target]) {
                    dist[e.target] = nd;
                    pq.push({nd, e.target});
                }
            }
        }

        return dist;
    }

    std::vector<float> dijkstra_reverse(NodeId source) const {
        std::vector<float> dist(n_, INF_DIST);
        using PQItem = std::pair<float, NodeId>;
        std::priority_queue<PQItem, std::vector<PQItem>, std::greater<>> pq;

        dist[source] = 0.0f;
        pq.push({0.0f, source});

        while (!pq.empty()) {
            auto [du, u] = pq.top();
            pq.pop();
            if (du != dist[u]) continue;

            for (const auto& e : reverse_[u]) {
                float nd = du + e.weight;
                if (nd < dist[e.target]) {
                    dist[e.target] = nd;
                    pq.push({nd, e.target});
                }
            }
        }

        return dist;
    }

    void build_upper_bound_tables(std::mt19937& rng) {
        ub_roots_ = sample_roots(rng, params_.num_upper_bound_roots);
        dist_from_roots_.clear();
        dist_to_roots_.clear();
        dist_from_roots_.reserve(ub_roots_.size());
        dist_to_roots_.reserve(ub_roots_.size());

        for (NodeId r : ub_roots_) {
            dist_from_roots_.push_back(dijkstra_forward(r));
            dist_to_roots_.push_back(dijkstra_reverse(r));
        }
    }

    void build_landmarks(std::mt19937& rng) {
        landmarks_.clear();
        landmark_dists_.clear();
        if (n_ == 0 || params_.num_landmarks == 0) return;

        uint32_t k = std::min<uint32_t>(params_.num_landmarks, static_cast<uint32_t>(n_));
        landmark_dists_.reserve(k);
        landmarks_.reserve(k);
        std::vector<uint8_t> used(n_, 0);

        if (!params_.landmarks_farthest) {
            landmarks_ = sample_anchors(rng, k);
            for (NodeId l : landmarks_) {
                landmark_dists_.push_back(undirected_dijkstra(l));
            }
            return;
        }

        NodeId seed = pick_seed_node(rng);
        auto dist_seed = undirected_dijkstra(seed);
        NodeId first = seed;
        float best = -1.0f;
        for (NodeId v = 0; v < n_; ++v) {
            float d = dist_seed[v];
            if (d >= INF_DIST * 0.5f) {
                first = v;
                best = d;
                break;
            }
            if (d > best) {
                best = d;
                first = v;
            }
        }

        landmarks_.push_back(first);
        used[first] = 1;
        landmark_dists_.push_back(std::move(dist_seed));

        std::vector<float> min_dist = landmark_dists_.back();

        while (landmarks_.size() < k) {
            NodeId next = INVALID_NODE;
            float far = -1.0f;
            for (NodeId v = 0; v < n_; ++v) {
                if (used[v]) continue;
                float d = min_dist[v];
                if (d >= INF_DIST * 0.5f) {
                    next = v;
                    far = d;
                    break;
                }
                if (d > far) {
                    far = d;
                    next = v;
                }
            }
            if (next == INVALID_NODE) break;

            auto dist_next = undirected_dijkstra(next);
            for (NodeId v = 0; v < n_; ++v) {
                if (dist_next[v] < min_dist[v]) {
                    min_dist[v] = dist_next[v];
                }
            }

            landmarks_.push_back(next);
            used[next] = 1;
            landmark_dists_.push_back(std::move(dist_next));
        }
    }

    float upper_bound(NodeId s, NodeId t) const {
        if (!params_.use_upper_bound || dist_from_roots_.empty()) return INF_DIST;
        float ub = INF_DIST;
        for (size_t i = 0; i < ub_roots_.size(); ++i) {
            float dsr = dist_to_roots_[i][s];
            float drt = dist_from_roots_[i][t];
            if (dsr < INF_DIST && drt < INF_DIST) {
                ub = std::min(ub, dsr + drt);
            }
        }
        return ub;
    }

    float heuristic_lips_component(NodeId v, NodeId t) const {
        float h = 0.0f;
        for (const auto& phi : potentials_) {
            if (phi[v] >= INF_DIST * 0.5f || phi[t] >= INF_DIST * 0.5f) {
                continue;  // Skip potentials that are disconnected for either node
            }
            float diff = std::fabs(phi[v] - phi[t]);
            if (diff > h) h = diff;
        }
        return h;
    }

    float heuristic_alt_component(NodeId v, NodeId t) const {
        float h = 0.0f;
        for (const auto& dist : landmark_dists_) {
            float dv = dist[v];
            float dt = dist[t];
            if (dv >= INF_DIST * 0.5f || dt >= INF_DIST * 0.5f) continue;
            float diff = std::fabs(dv - dt);
            if (diff > h) h = diff;
        }
        return h;
    }

    float heuristic(NodeId v, NodeId t) const {
        float lips = heuristic_lips_component(v, t);
        float alt = heuristic_alt_component(v, t);
        return (lips > alt) ? lips : alt;
    }
};

} // namespace lops
} // namespace photon
