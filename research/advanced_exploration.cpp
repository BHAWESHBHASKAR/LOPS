/**
 * FOCUSED FORMULA REFINEMENT
 * 
 * Based on our experiments:
 * - Entropy approach works best on scale-free/social networks
 * - Bidirectional alone provides good speedups on certain graphs
 * - The key insight is hub preference
 * 
 * Let's try more radical approaches and hybrid strategies.
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
#include <unordered_set>

using namespace photon;

namespace advanced {

// =============================================================================
// NOVEL APPROACH 1: LANDMARK HEURISTIC (Zero-prep version)
// Select random "hub" nodes, compute distances to them, use as heuristic
// =============================================================================

struct LandmarkHeuristic {
    std::vector<std::vector<float>> landmark_dists;  // [landmark_idx][node] = dist
    std::vector<NodeId> landmarks;
    NodeId target;
    
    static LandmarkHeuristic create(const Graph& graph, NodeId target, int num_landmarks = 4) {
        LandmarkHeuristic h;
        h.target = target;
        
        const size_t n = graph.num_nodes();
        
        // Select landmarks: top-degree nodes
        std::vector<std::pair<size_t, NodeId>> degree_nodes;
        for (NodeId v = 0; v < n; ++v) {
            degree_nodes.push_back({graph.out_degree(v), v});
        }
        std::sort(degree_nodes.rbegin(), degree_nodes.rend());
        
        for (int i = 0; i < num_landmarks && i < (int)n; ++i) {
            h.landmarks.push_back(degree_nodes[i].second);
        }
        
        // Compute distances from each landmark (BFS/Dijkstra)
        h.landmark_dists.resize(num_landmarks);
        for (int i = 0; i < num_landmarks; ++i) {
            h.landmark_dists[i].resize(n, INF_DIST);
            
            std::priority_queue<std::pair<float, NodeId>,
                std::vector<std::pair<float, NodeId>>,
                std::greater<>> pq;
            
            h.landmark_dists[i][h.landmarks[i]] = 0;
            pq.push({0, h.landmarks[i]});
            
            while (!pq.empty()) {
                auto [d, u] = pq.top();
                pq.pop();
                
                if (d > h.landmark_dists[i][u]) continue;
                
                for (const auto& e : graph.out_edges(u)) {
                    float nd = d + e.weight;
                    if (nd < h.landmark_dists[i][e.target]) {
                        h.landmark_dists[i][e.target] = nd;
                        pq.push({nd, e.target});
                    }
                }
            }
        }
        
        return h;
    }
    
    float operator()(NodeId v, float g) const {
        // ALT-style heuristic: max over landmarks of |d(v,L) - d(t,L)|
        float h = 0;
        for (size_t i = 0; i < landmarks.size(); ++i) {
            float diff = std::abs(landmark_dists[i][v] - landmark_dists[i][target]);
            h = std::max(h, diff);
        }
        return g + h;  // A*-style: f = g + h
    }
};

PathResult landmark_astar(const Graph& graph, NodeId source, NodeId target, int num_landmarks = 4) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "Landmark-A*";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    auto h = LandmarkHeuristic::create(graph, target, num_landmarks);
    
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>> pq;
    
    g_score[source] = 0;
    pq.push({h(source, 0), source});
    
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
                pq.push({h(e.target, new_g), e.target});
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

// =============================================================================
// NOVEL APPROACH 2: DEGREE-WEIGHTED EDGE RELAXATION
// When relaxing edges, prefer edges pointing to high-degree nodes
// =============================================================================

PathResult degree_edge_preference(const Graph& graph, NodeId source, NodeId target, float alpha = 0.1f) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "Degree-Edge-Prefer";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Precompute log-degree
    float max_log_deg = 0;
    std::vector<float> log_deg(n);
    for (NodeId v = 0; v < n; ++v) {
        log_deg[v] = std::log2(graph.out_degree(v) + 1.0f);
        max_log_deg = std::max(max_log_deg, log_deg[v]);
    }
    
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>> pq;
    
    g_score[source] = 0;
    pq.push({-alpha * log_deg[source] / max_log_deg, source});
    
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
                // Priority: true distance minus small degree bonus
                float priority = new_g - alpha * log_deg[e.target] / max_log_deg;
                pq.push({priority, e.target});
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

// =============================================================================
// NOVEL APPROACH 3: HUB-FIRST SEARCH
// Quickly reach a hub, then search from hub to target
// =============================================================================

PathResult hub_first_search(const Graph& graph, NodeId source, NodeId target, float hub_threshold = 0.8f) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "Hub-First";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Find max degree and hub threshold
    float max_deg = 0;
    for (NodeId v = 0; v < n; ++v) {
        max_deg = std::max(max_deg, (float)graph.out_degree(v));
    }
    float hub_deg = hub_threshold * max_deg;
    
    // Phase 1: Quick search towards any hub
    std::vector<float> dist_s(n, INF_DIST);
    std::vector<NodeId> parent_s(n, INVALID_NODE);
    std::priority_queue<std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>> pq_s;
    
    dist_s[source] = 0;
    pq_s.push({0, source});
    NodeId best_hub = INVALID_NODE;
    float best_hub_dist = INF_DIST;
    
    std::vector<bool> visited_s(n, false);
    while (!pq_s.empty() && best_hub == INVALID_NODE) {
        auto [d, u] = pq_s.top();
        pq_s.pop();
        
        if (visited_s[u]) continue;
        visited_s[u] = true;
        result.nodes_explored++;
        
        if (graph.out_degree(u) >= hub_deg) {
            best_hub = u;
            best_hub_dist = dist_s[u];
            break;
        }
        
        for (const auto& e : graph.out_edges(u)) {
            result.edges_relaxed++;
            float nd = dist_s[u] + e.weight;
            if (nd < dist_s[e.target]) {
                dist_s[e.target] = nd;
                parent_s[e.target] = u;
                // Prefer going towards higher degree nodes
                float deg_bonus = graph.out_degree(e.target) / max_deg * 0.5f;
                pq_s.push({nd - deg_bonus, e.target});
            }
        }
    }
    
    // Phase 2: Dijkstra from hub to target (if hub found)
    if (best_hub != INVALID_NODE) {
        std::vector<float> dist_h(n, INF_DIST);
        std::vector<NodeId> parent_h(n, INVALID_NODE);
        std::vector<bool> visited_h(n, false);
        
        std::priority_queue<std::pair<float, NodeId>,
            std::vector<std::pair<float, NodeId>>,
            std::greater<>> pq_h;
        
        dist_h[best_hub] = 0;
        pq_h.push({0, best_hub});
        
        while (!pq_h.empty()) {
            auto [d, u] = pq_h.top();
            pq_h.pop();
            
            if (visited_h[u]) continue;
            visited_h[u] = true;
            result.nodes_explored++;
            
            if (u == target) break;
            
            for (const auto& e : graph.out_edges(u)) {
                result.edges_relaxed++;
                float nd = dist_h[u] + e.weight;
                if (nd < dist_h[e.target]) {
                    dist_h[e.target] = nd;
                    parent_h[e.target] = u;
                    pq_h.push({nd, e.target});
                }
            }
        }
        
        // Reconstruct path: source -> hub -> target
        if (dist_h[target] < INF_DIST) {
            // Path from source to hub
            std::vector<NodeId> path1;
            NodeId curr = best_hub;
            while (curr != INVALID_NODE) {
                path1.push_back(curr);
                curr = parent_s[curr];
            }
            std::reverse(path1.begin(), path1.end());
            
            // Path from hub to target
            curr = target;
            std::vector<NodeId> path2;
            while (curr != best_hub) {
                path2.push_back(curr);
                curr = parent_h[curr];
            }
            std::reverse(path2.begin(), path2.end());
            
            result.path = std::move(path1);
            for (NodeId v : path2) result.path.push_back(v);
            result.distance = best_hub_dist + dist_h[target];
        }
    }
    
    // Fallback: normal Dijkstra if hub approach fails or is suboptimal
    if (result.distance >= INF_DIST || result.path.empty()) {
        auto fallback = search::dijkstra(graph, source, target);
        result.distance = fallback.distance;
        result.path = fallback.path;
        result.nodes_explored += fallback.nodes_explored;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.time_microseconds = std::chrono::duration<double, std::micro>(
        end_time - start_time).count();
    
    return result;
}

// =============================================================================
// NOVEL APPROACH 4: GRADIENT DESCENT ON DEGREE FIELD
// Move towards direction of increasing degree until hitting a hub, then search
// =============================================================================

PathResult gradient_degree_search(const Graph& graph, NodeId source, NodeId target) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "Gradient-Degree";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Use Dijkstra but with a modified priority that encourages
    // exploring high-degree nodes first, especially early on
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    float max_deg = 0;
    for (NodeId v = 0; v < n; ++v) {
        max_deg = std::max(max_deg, (float)graph.out_degree(v));
    }
    
    std::priority_queue<std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>> pq;
    
    g_score[source] = 0;
    pq.push({0, source});
    
    int expansion_count = 0;
    int phase_threshold = n / 10;  // First 10% of expansions: strong degree preference
    
    while (!pq.empty()) {
        auto [f, u] = pq.top();
        pq.pop();
        
        if (visited[u]) continue;
        visited[u] = true;
        result.nodes_explored++;
        expansion_count++;
        
        if (u == target) break;
        
        for (const auto& e : graph.out_edges(u)) {
            result.edges_relaxed++;
            float new_g = g_score[u] + e.weight;
            
            if (new_g < g_score[e.target]) {
                g_score[e.target] = new_g;
                parent[e.target] = u;
                
                // Early phase: strong degree preference
                // Late phase: pure Dijkstra
                float degree_bonus = 0;
                if (expansion_count < phase_threshold) {
                    degree_bonus = 0.5f * std::log2(graph.out_degree(e.target) + 1.0f) / std::log2(max_deg + 1.0f);
                }
                
                pq.push({new_g - degree_bonus, e.target});
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

// =============================================================================
// NOVEL APPROACH 5: MULTI-LEVEL HEURISTIC
// Different heuristics at different search depths
// =============================================================================

PathResult multilevel_search(const Graph& graph, NodeId source, NodeId target) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    PathResult result;
    result.strategy_used = "Multi-Level";
    
    const size_t n = graph.num_nodes();
    
    if (source == target) {
        result.distance = 0;
        result.path = {source};
        return result;
    }
    
    // Precompute features
    float max_deg = 0;
    float avg_deg = 0;
    std::vector<float> log_deg(n);
    for (NodeId v = 0; v < n; ++v) {
        log_deg[v] = std::log2(graph.out_degree(v) + 1.0f);
        max_deg = std::max(max_deg, (float)graph.out_degree(v));
        avg_deg += graph.out_degree(v);
    }
    avg_deg /= n;
    float log_max_deg = std::log2(max_deg + 1.0f);
    
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<std::tuple<float, float, NodeId>,  // (priority, depth, node)
        std::vector<std::tuple<float, float, NodeId>>,
        std::greater<>> pq;
    
    g_score[source] = 0;
    pq.push({-log_deg[source] / log_max_deg * 0.3f, 0, source});
    
    while (!pq.empty()) {
        auto [f, depth, u] = pq.top();
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
                
                // Multi-level heuristic
                float new_depth = depth + 1;
                float alpha;
                if (new_depth < 3) {
                    // Early: strong degree preference
                    alpha = 0.3f;
                } else if (new_depth < 10) {
                    // Mid: moderate preference
                    alpha = 0.15f;
                } else {
                    // Late: pure Dijkstra
                    alpha = 0.05f;
                }
                
                float priority = new_g - alpha * log_deg[e.target] / log_max_deg;
                pq.push({priority, new_depth, e.target});
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

} // namespace advanced

// =============================================================================
// EXPERIMENT
// =============================================================================

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════════════════╗
║                         ADVANCED ALGORITHM EXPLORATION                             ║
║                                                                                    ║
║  Testing novel approaches:                                                         ║
║  1. Landmark A* (on-the-fly landmark selection)                                    ║
║  2. Degree-Edge Preference (reorder edge relaxation)                               ║
║  3. Hub-First Search (reach hub, then to target)                                   ║
║  4. Gradient Degree Search (follow increasing degree)                              ║
║  5. Multi-Level Search (different heuristics at different depths)                  ║
╚════════════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Test on graphs where we expect degree-based heuristics to work
    std::vector<std::pair<std::string, Graph>> test_graphs;
    test_graphs.push_back({"Social 2000", generators::social_network(2000, 12)});
    test_graphs.push_back({"Scale-Free 1500", generators::scale_free(1500, 2.3f)});
    test_graphs.push_back({"Road 1500", generators::road_network(1500, 6)});
    test_graphs.push_back({"Grid 35x35", generators::grid(35, 35, true, true)});
    
    std::map<std::string, std::vector<double>> all_speedups;
    std::map<std::string, std::vector<double>> all_opts;
    std::map<std::string, int> wins;
    
    for (auto& [graph_name, graph] : test_graphs) {
        std::cout << "\n═══════════════════════════════════════════════════════════════════════════════\n";
        std::cout << "📊 " << graph_name << " (" << graph.num_nodes() << " nodes)\n";
        std::cout << "═══════════════════════════════════════════════════════════════════════════════\n\n";
        
        // Generate queries
        const size_t num_queries = 30;
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
        
        std::string best_algo = "";
        double best_speedup = 0;
        
        auto test_algo = [&](const char* name, auto&& func) {
            double total_time = 0, total_nodes = 0, total_ratio = 0;
            int optimal_count = 0;
            
            for (size_t i = 0; i < queries.size(); ++i) {
                auto [s, t] = queries[i];
                auto r = func(graph, s, t);
                
                total_time += r.time_microseconds;
                total_nodes += r.nodes_explored;
                
                if (r.found() && optimal_dist[i] < INF_DIST) {
                    double ratio = r.distance / optimal_dist[i];
                    total_ratio += ratio;
                    if (ratio <= 1.001) optimal_count++;
                }
            }
            
            double avg_time = total_time / num_queries;
            double avg_nodes = total_nodes / num_queries;
            double avg_ratio = total_ratio / num_queries;
            double sp = dij_time / avg_time;
            double node_red = (dij_nodes - avg_nodes) / dij_nodes * 100;
            
            all_speedups[name].push_back(sp);
            all_opts[name].push_back(avg_ratio);
            
            bool all_opt = (optimal_count == (int)num_queries);
            if (all_opt && sp > best_speedup) {
                best_speedup = sp;
                best_algo = name;
            }
            
            std::cout << std::left << std::setw(25) << name
                      << " │ " << std::right << std::setw(6) << std::fixed << std::setprecision(2) << sp << "×"
                      << " │ " << std::setw(5) << std::setprecision(3) << avg_ratio
                      << " │ " << optimal_count << "/" << num_queries
                      << " │ " << std::showpos << std::setw(5) << std::setprecision(0) << node_red << "%" 
                      << std::noshowpos << "\n";
        };
        
        std::cout << std::left << std::setw(25) << "Algorithm"
                  << " │ Speedup │ Ratio │ Opt  │ Nodes\n";
        std::cout << "────────────────────────────────────────────────────────────────────────\n";
        std::cout << std::left << std::setw(25) << "Dijkstra"
                  << " │   1.00× │ 1.000 │ " << num_queries << "/" << num_queries << " │   +0%\n";
        
        test_algo("Landmark-A* (2)", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::landmark_astar(g, s, t, 2);
        });
        test_algo("Landmark-A* (4)", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::landmark_astar(g, s, t, 4);
        });
        test_algo("Degree-Edge a=0.1", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::degree_edge_preference(g, s, t, 0.1f);
        });
        test_algo("Degree-Edge a=0.2", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::degree_edge_preference(g, s, t, 0.2f);
        });
        test_algo("Degree-Edge a=0.3", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::degree_edge_preference(g, s, t, 0.3f);
        });
        test_algo("Hub-First 0.8", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::hub_first_search(g, s, t, 0.8f);
        });
        test_algo("Hub-First 0.5", [](const Graph& g, NodeId s, NodeId t) {
            return advanced::hub_first_search(g, s, t, 0.5f);
        });
        test_algo("Gradient-Degree", advanced::gradient_degree_search);
        test_algo("Multi-Level", advanced::multilevel_search);
        
        std::cout << "────────────────────────────────────────────────────────────────────────\n";
        if (!best_algo.empty()) {
            std::cout << "★ Best optimal: " << best_algo << " (" << best_speedup << "×)\n";
            wins[best_algo]++;
        }
    }
    
    // Final summary
    std::cout << "\n\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                 SUMMARY                                            ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Algorithm                   │ Avg Speedup │ Avg Opt │ Wins ║\n";
    std::cout << "╠════════════════════════════════════════════════════════════════════════════════════╣\n";
    
    std::vector<std::tuple<std::string, double, double, int>> ranked;
    for (const auto& [name, sps] : all_speedups) {
        double avg_sp = 0, avg_opt = 0;
        for (double s : sps) avg_sp += s;
        avg_sp /= sps.size();
        for (double o : all_opts[name]) avg_opt += o;
        avg_opt /= all_opts[name].size();
        ranked.push_back({name, avg_sp, avg_opt, wins[name]});
    }
    
    std::sort(ranked.begin(), ranked.end(), [](auto& a, auto& b) {
        bool a_opt = std::get<2>(a) <= 1.001;
        bool b_opt = std::get<2>(b) <= 1.001;
        if (a_opt != b_opt) return a_opt > b_opt;
        return std::get<1>(a) > std::get<1>(b);
    });
    
    for (const auto& [name, sp, opt, w] : ranked) {
        std::cout << "║ " << std::left << std::setw(27) << name
                  << " │ " << std::right << std::setw(10) << std::fixed << std::setprecision(2) << sp << "×"
                  << " │ " << std::setw(7) << std::setprecision(4) << opt
                  << " │ " << std::setw(4) << w << " ║\n";
    }
    
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
