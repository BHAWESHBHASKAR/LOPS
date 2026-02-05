/**
 * BIDIRECTIONAL FORMULA EXPLORATION
 * 
 * The bidirectional entropy approach showed 5-6× speedups.
 * Let's explore many variations of the bidirectional formula.
 */

#include "../include/photon/photon.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <set>
#include <queue>
#include <random>
#include <cmath>
#include <functional>

using namespace photon;

// =============================================================================
// BIDIRECTIONAL WITH CUSTOMIZABLE HEURISTICS
// =============================================================================

namespace bidir {

// Graph features for heuristics
struct Features {
    std::vector<float> degree;
    std::vector<float> log_degree;
    std::vector<float> pagerank;
    float max_degree;
    float avg_degree;
    
    static Features compute(const Graph& graph, int pr_iterations = 5) {
        Features f;
        const size_t n = graph.num_nodes();
        f.degree.resize(n);
        f.log_degree.resize(n);
        f.pagerank.resize(n, 1.0f / n);
        f.max_degree = 0;
        f.avg_degree = 0;
        
        for (NodeId v = 0; v < n; ++v) {
            f.degree[v] = graph.out_degree(v);
            f.log_degree[v] = std::log2(f.degree[v] + 1.0f);
            f.max_degree = std::max(f.max_degree, f.degree[v]);
            f.avg_degree += f.degree[v];
        }
        f.avg_degree /= n;
        
        // PageRank
        std::vector<float> pr_new(n);
        float damping = 0.85f;
        for (int iter = 0; iter < pr_iterations; ++iter) {
            std::fill(pr_new.begin(), pr_new.end(), (1.0f - damping) / n);
            for (NodeId v = 0; v < n; ++v) {
                auto edges = graph.out_edges(v);
                if (edges.empty()) continue;
                float contrib = damping * f.pagerank[v] / edges.size();
                for (const auto& e : edges) {
                    pr_new[e.target] += contrib;
                }
            }
            std::swap(f.pagerank, pr_new);
        }
        
        float pr_max = *std::max_element(f.pagerank.begin(), f.pagerank.end());
        if (pr_max > 0) {
            for (float& pr : f.pagerank) pr /= pr_max;
        }
        
        return f;
    }
};

// Generic bidirectional search with heuristic
template<typename HeuristicFunc>
PathResult bidirectional_heuristic(
    const Graph& graph,
    NodeId source,
    NodeId target,
    HeuristicFunc&& h_func,
    const char* name
) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = name;
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Forward and backward data structures
    std::vector<float> dist_fwd(n, INF_DIST);
    std::vector<float> dist_bwd(n, INF_DIST);
    std::vector<NodeId> parent_fwd(n, INVALID_NODE);
    std::vector<NodeId> parent_bwd(n, INVALID_NODE);
    std::vector<bool> visited_fwd(n, false);
    std::vector<bool> visited_bwd(n, false);
    
    std::priority_queue<std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>> pq_fwd, pq_bwd;
    
    dist_fwd[source] = 0;
    dist_bwd[target] = 0;
    pq_fwd.push({h_func(source, 0), source});
    pq_bwd.push({h_func(target, 0), target});
    
    float best_dist = INF_DIST;
    NodeId meeting_point = INVALID_NODE;
    
    // Alternating expansion
    while (!pq_fwd.empty() || !pq_bwd.empty()) {
        // Forward step
        if (!pq_fwd.empty()) {
            auto [f, u] = pq_fwd.top();
            pq_fwd.pop();
            
            if (!visited_fwd[u] && dist_fwd[u] < best_dist) {
                visited_fwd[u] = true;
                result.nodes_explored++;
                
                // Check meeting
                if (dist_bwd[u] < INF_DIST) {
                    float total = dist_fwd[u] + dist_bwd[u];
                    if (total < best_dist) {
                        best_dist = total;
                        meeting_point = u;
                    }
                }
                
                for (const auto& e : graph.out_edges(u)) {
                    result.edges_relaxed++;
                    float new_dist = dist_fwd[u] + e.weight;
                    if (new_dist < dist_fwd[e.target]) {
                        dist_fwd[e.target] = new_dist;
                        parent_fwd[e.target] = u;
                        pq_fwd.push({h_func(e.target, new_dist), e.target});
                    }
                }
            }
        }
        
        // Backward step
        if (!pq_bwd.empty()) {
            auto [f, u] = pq_bwd.top();
            pq_bwd.pop();
            
            if (!visited_bwd[u] && dist_bwd[u] < best_dist) {
                visited_bwd[u] = true;
                result.nodes_explored++;
                
                // Check meeting
                if (dist_fwd[u] < INF_DIST) {
                    float total = dist_fwd[u] + dist_bwd[u];
                    if (total < best_dist) {
                        best_dist = total;
                        meeting_point = u;
                    }
                }
                
                for (const auto& e : graph.out_edges(u)) {
                    result.edges_relaxed++;
                    float new_dist = dist_bwd[u] + e.weight;
                    if (new_dist < dist_bwd[e.target]) {
                        dist_bwd[e.target] = new_dist;
                        parent_bwd[e.target] = u;
                        pq_bwd.push({h_func(e.target, new_dist), e.target});
                    }
                }
            }
        }
        
        // Early termination
        if (!pq_fwd.empty() && !pq_bwd.empty()) {
            float min_fwd = pq_fwd.top().first;
            float min_bwd = pq_bwd.top().first;
            // More aggressive termination
            if (min_fwd >= best_dist && min_bwd >= best_dist) break;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    if (meeting_point != INVALID_NODE) {
        // Reconstruct path
        std::vector<NodeId> path1, path2;
        
        NodeId curr = meeting_point;
        while (curr != INVALID_NODE) {
            path1.push_back(curr);
            curr = parent_fwd[curr];
        }
        std::reverse(path1.begin(), path1.end());
        
        curr = parent_bwd[meeting_point];
        while (curr != INVALID_NODE) {
            path2.push_back(curr);
            curr = parent_bwd[curr];
        }
        
        result.path = std::move(path1);
        for (NodeId v : path2) {
            result.path.push_back(v);
        }
        result.distance = best_dist;
    } else {
        result.distance = INF_DIST;
    }
    
    return result;
}

// =============================================================================
// HEURISTIC FORMULAS FOR BIDIRECTIONAL SEARCH
// =============================================================================

// B1: No heuristic (pure bidirectional Dijkstra)
struct B_NoHeuristic {
    float operator()(NodeId v, float g) const {
        return g;
    }
};

// B2: Log-degree bonus
struct B_LogDegree {
    const Features& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * f.log_degree[v];
    }
};

// B3: Normalized degree bonus
struct B_NormDegree {
    const Features& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * (f.degree[v] / f.max_degree);
    }
};

// B4: PageRank bonus
struct B_PageRank {
    const Features& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * f.pagerank[v];
    }
};

// B5: Combined degree + PageRank
struct B_DegreePageRank {
    const Features& f;
    float w_deg, w_pr;
    
    float operator()(NodeId v, float g) const {
        return g - w_deg * f.log_degree[v] - w_pr * f.pagerank[v];
    }
};

// B6: Distance-adaptive (entropy matters more as we go deeper)
struct B_DistanceAdaptive {
    const Features& f;
    float base_weight;
    
    float operator()(NodeId v, float g) const {
        float scale = std::min(1.0f, g / 10.0f);  // Scale up with distance
        return g - base_weight * scale * f.log_degree[v];
    }
};

// B7: Hub preference (prefer reaching hubs early)
struct B_HubPreference {
    const Features& f;
    float threshold;
    float bonus;
    
    float operator()(NodeId v, float g) const {
        // Large bonus for hub nodes (degree > threshold)
        float hub_bonus = (f.degree[v] > threshold) ? bonus : 0;
        return g - hub_bonus;
    }
};

// B8: Sqrt-degree
struct B_SqrtDegree {
    const Features& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * std::sqrt(f.degree[v]) / std::sqrt(f.max_degree);
    }
};

// B9: Inverse clustering (prefer bridges)
struct B_InverseClustering {
    const Graph& graph;
    const Features& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        // Approximate: low clustering = more bridge-like
        // For simplicity, use 1/(local clustering estimate + 1)
        auto edges = graph.out_edges(v);
        if (edges.size() < 2) return g;
        
        // Quick check for triangles
        std::set<NodeId> neighbors;
        for (const auto& e : edges) neighbors.insert(e.target);
        
        int triangles = 0;
        for (const auto& e : edges) {
            for (const auto& e2 : graph.out_edges(e.target)) {
                if (neighbors.count(e2.target)) triangles++;
            }
            if (triangles > 5) break;  // Early exit
        }
        
        float clust = triangles / (edges.size() * (edges.size() - 1) / 2.0f + 1.0f);
        return g - weight * (1.0f - clust);  // Prefer low clustering
    }
};

// B10: Degree percentile (top 10% get big bonus)
struct B_DegreePercentile {
    const Features& f;
    float percentile_threshold;
    float top_bonus;
    float normal_weight;
    
    float operator()(NodeId v, float g) const {
        float norm_deg = f.degree[v] / f.max_degree;
        if (norm_deg >= percentile_threshold) {
            return g - top_bonus;
        }
        return g - normal_weight * norm_deg;
    }
};

} // namespace bidir

// =============================================================================
// MAIN EXPERIMENT
// =============================================================================

int main() {
    using namespace bidir;
    
    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════════════════╗
║                    BIDIRECTIONAL FORMULA DEEP EXPLORATION                          ║
║                                                                                    ║
║  Testing bidirectional variants - these showed the best speedups earlier           ║
╚════════════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Test graphs - focus on scale-free and social where bidir shines
    std::vector<std::pair<std::string, Graph>> test_graphs;
    test_graphs.push_back({"Social 5000", generators::social_network(5000, 15)});
    test_graphs.push_back({"Scale-Free 4000", generators::scale_free(4000, 2.3f)});
    test_graphs.push_back({"Road 3000", generators::road_network(3000, 7)});
    test_graphs.push_back({"Grid 60x60", generators::grid(60, 60, true, true)});
    test_graphs.push_back({"Random 2000", generators::random_graph(2000, 0.01f, true)});
    
    // Results tracking
    std::map<std::string, std::vector<double>> speedups;
    std::map<std::string, std::vector<double>> opt_ratios;
    std::map<std::string, int> wins;
    
    for (auto& [graph_name, graph] : test_graphs) {
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════════════════════════════════════════════\n";
        std::cout << "📊 " << graph_name << " (" << graph.num_nodes() << " nodes, " 
                  << graph.num_edges() << " edges)\n";
        std::cout << "═══════════════════════════════════════════════════════════════════════════════\n";
        
        // Compute features
        auto features = Features::compute(graph);
        std::cout << "  max_degree=" << features.max_degree 
                  << " avg_degree=" << std::fixed << std::setprecision(1) << features.avg_degree << "\n\n";
        
        // Generate queries
        const size_t num_queries = 50;
        std::mt19937 rng(42);
        std::uniform_int_distribution<NodeId> dist(0, graph.num_nodes() - 1);
        std::vector<std::pair<NodeId, NodeId>> queries;
        for (size_t i = 0; i < num_queries; ++i) {
            NodeId s = dist(rng), t = dist(rng);
            while (t == s) t = dist(rng);
            queries.push_back({s, t});
        }
        
        // Dijkstra baseline
        double dij_time = 0, dij_nodes = 0;
        std::vector<float> optimal_dist;
        for (auto [s, t] : queries) {
            auto r = search::dijkstra(graph, s, t);
            dij_time += r.time_microseconds;
            dij_nodes += r.nodes_explored;
            optimal_dist.push_back(r.distance);
        }
        dij_time /= num_queries;
        dij_nodes /= num_queries;
        
        // Best tracker for this graph
        std::string best_formula = "";
        double best_speedup = 0;
        
        auto test_formula = [&](const char* name, auto&& heuristic_factory) {
            double total_time = 0, total_nodes = 0, total_ratio = 0;
            bool all_optimal = true;
            int optimal_count = 0;
            
            for (size_t i = 0; i < queries.size(); ++i) {
                auto [s, t] = queries[i];
                auto h = heuristic_factory();
                auto r = bidirectional_heuristic(graph, s, t, h, name);
                
                total_time += r.time_microseconds;
                total_nodes += r.nodes_explored;
                
                if (r.found() && optimal_dist[i] < INF_DIST) {
                    double ratio = r.distance / optimal_dist[i];
                    total_ratio += ratio;
                    if (ratio <= 1.001) optimal_count++;
                    else all_optimal = false;
                } else if (!r.found() && optimal_dist[i] < INF_DIST) {
                    all_optimal = false;
                }
            }
            
            double avg_time = total_time / num_queries;
            double avg_nodes = total_nodes / num_queries;
            double avg_ratio = total_ratio / num_queries;
            double sp = dij_time / avg_time;
            double node_reduction = (dij_nodes - avg_nodes) / dij_nodes * 100;
            
            speedups[name].push_back(sp);
            opt_ratios[name].push_back(avg_ratio);
            
            if (all_optimal && sp > best_speedup) {
                best_speedup = sp;
                best_formula = name;
            }
            
            std::cout << std::left << std::setw(28) << name 
                      << " │ " << std::right << std::setw(6) << std::fixed << std::setprecision(2) << sp << "×"
                      << " │ " << std::setw(5) << std::setprecision(3) << avg_ratio
                      << " │ " << optimal_count << "/" << num_queries
                      << " │ " << std::showpos << std::setw(5) << std::setprecision(0) << node_reduction << "%" 
                      << std::noshowpos << "\n";
        };
        
        std::cout << std::left << std::setw(28) << "Formula" 
                  << " │ Speedup │ Ratio │ Opt  │ Nodes\n";
        std::cout << "──────────────────────────────────────────────────────────────────────────────\n";
        
        // Dijkstra baseline
        std::cout << std::left << std::setw(28) << "Dijkstra" 
                  << " │   1.00× │ 1.000 │ " << num_queries << "/" << num_queries << " │   +0%\n";
        
        // Pure bidirectional
        test_formula("Bidir-No-Heuristic", [&]() { 
            return B_NoHeuristic{}; 
        });
        
        // Log-degree variations
        for (float w : {0.1f, 0.2f, 0.3f, 0.5f, 0.7f}) {
            std::string name = "Bidir-LogDeg w=" + std::to_string(w).substr(0,3);
            test_formula(name.c_str(), [&, w]() { 
                return B_LogDegree{features, w}; 
            });
        }
        
        // Normalized degree
        for (float w : {0.2f, 0.5f, 1.0f}) {
            std::string name = "Bidir-NormDeg w=" + std::to_string(w).substr(0,3);
            test_formula(name.c_str(), [&, w]() { 
                return B_NormDegree{features, w}; 
            });
        }
        
        // PageRank
        for (float w : {0.2f, 0.5f, 1.0f}) {
            std::string name = "Bidir-PR w=" + std::to_string(w).substr(0,3);
            test_formula(name.c_str(), [&, w]() { 
                return B_PageRank{features, w}; 
            });
        }
        
        // Combined
        test_formula("Bidir-Deg+PR", [&]() { 
            return B_DegreePageRank{features, 0.2f, 0.3f}; 
        });
        
        // Distance-adaptive
        test_formula("Bidir-DistAdaptive", [&]() { 
            return B_DistanceAdaptive{features, 0.3f}; 
        });
        
        // Hub preference
        float hub_thresh = features.avg_degree * 3;
        test_formula("Bidir-HubPrefer", [&, hub_thresh]() { 
            return B_HubPreference{features, hub_thresh, 0.5f}; 
        });
        
        // Sqrt degree
        test_formula("Bidir-SqrtDeg", [&]() { 
            return B_SqrtDegree{features, 0.3f}; 
        });
        
        // Percentile
        test_formula("Bidir-Top10%Bonus", [&]() { 
            return B_DegreePercentile{features, 0.9f, 1.0f, 0.1f}; 
        });
        
        std::cout << "──────────────────────────────────────────────────────────────────────────────\n";
        if (!best_formula.empty()) {
            std::cout << "★ Best optimal: " << best_formula << " (" << std::setprecision(2) << best_speedup << "×)\n";
            wins[best_formula]++;
        }
    }
    
    // Final summary
    std::cout << "\n\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                 FINAL SUMMARY                                      ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════╣\n";
    
    // Compute averages and rank
    std::vector<std::tuple<std::string, double, double, int>> rankings;
    for (const auto& [name, sps] : speedups) {
        double avg_sp = 0, avg_opt = 0;
        for (double s : sps) avg_sp += s;
        avg_sp /= sps.size();
        for (double o : opt_ratios[name]) avg_opt += o;
        avg_opt /= opt_ratios[name].size();
        rankings.push_back({name, avg_sp, avg_opt, wins[name]});
    }
    
    // Sort by speedup among near-optimal
    std::sort(rankings.begin(), rankings.end(), [](auto& a, auto& b) {
        bool a_opt = std::get<2>(a) < 1.01;
        bool b_opt = std::get<2>(b) < 1.01;
        if (a_opt != b_opt) return a_opt > b_opt;
        return std::get<1>(a) > std::get<1>(b);
    });
    
    std::cout << "║ " << std::left << std::setw(28) << "Formula" 
              << " │ Avg Speedup │ Avg Opt │ Wins ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════╣\n";
    
    for (const auto& [name, sp, opt, w] : rankings) {
        std::cout << "║ " << std::left << std::setw(28) << name 
                  << " │ " << std::right << std::setw(10) << std::fixed << std::setprecision(2) << sp << "×"
                  << " │ " << std::setw(7) << std::setprecision(4) << opt
                  << " │ " << std::setw(4) << w << " ║\n";
    }
    
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════════╝\n";
    
    // Champion
    auto& champion = rankings[0];
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                   🏆 CHAMPION                                      ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║                                                                                    ║\n";
    std::cout << "║  Formula:     " << std::left << std::setw(60) << std::get<0>(champion) << "    ║\n";
    std::cout << "║  Avg Speedup: " << std::right << std::setw(6) << std::fixed << std::setprecision(2) 
              << std::get<1>(champion) << "×                                                          ║\n";
    std::cout << "║  Optimality:  " << std::setw(6) << std::setprecision(4) << std::get<2>(champion) 
              << "                                                          ║\n";
    std::cout << "║                                                                                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
