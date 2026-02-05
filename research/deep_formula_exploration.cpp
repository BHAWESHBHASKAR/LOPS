/**
 * DEEP FORMULA EXPLORATION
 * 
 * We're going to test MANY variations of the entropy/degree heuristic
 * to find the optimal formulation.
 * 
 * Variables to explore:
 * 1. Entropy function variants (log, sqrt, linear, polynomial)
 * 2. Normalization strategies
 * 3. Combining multiple graph features
 * 4. Distance-dependent scaling
 * 5. Graph-adaptive weight selection
 * 6. Bidirectional asymmetric weights
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
// FORMULA VARIATIONS TO TEST
// =============================================================================

namespace formulas {

// Base modified Dijkstra with customizable heuristic
template<typename HeuristicFunc>
PathResult priority_dijkstra(
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
    
    std::vector<float> g_score(n, INF_DIST);
    std::vector<NodeId> parent(n, INVALID_NODE);
    std::vector<bool> visited(n, false);
    
    std::priority_queue<
        std::pair<float, NodeId>,
        std::vector<std::pair<float, NodeId>>,
        std::greater<>
    > pq;
    
    g_score[source] = 0;
    pq.push({h_func(source, 0), source});
    
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
                pq.push({h_func(e.target, new_g), e.target});
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
// ENTROPY FUNCTION VARIANTS
// =============================================================================

// F1: Original log entropy
inline float entropy_log(float degree) {
    return std::log2(degree + 1.0f);
}

// F2: Square root (gentler curve)
inline float entropy_sqrt(float degree) {
    return std::sqrt(degree);
}

// F3: Linear (raw degree)
inline float entropy_linear(float degree) {
    return degree;
}

// F4: Polynomial (degree^0.7 - between sqrt and linear)
inline float entropy_poly(float degree) {
    return std::pow(degree + 1.0f, 0.7f);
}

// F5: Double log (very gentle)
inline float entropy_loglog(float degree) {
    return std::log2(std::log2(degree + 2.0f) + 1.0f);
}

// F6: Sigmoid-like (saturates for very high degree)
inline float entropy_sigmoid(float degree, float k = 10.0f) {
    return degree / (degree + k);
}

// F7: Inverse (penalize high degree instead!)
inline float entropy_inverse(float degree) {
    return 1.0f / (degree + 1.0f);
}

// F8: Log-ratio (relative to average)
inline float entropy_logratio(float degree, float avg_degree) {
    return std::log2((degree + 1.0f) / (avg_degree + 1.0f));
}

// =============================================================================
// COMBINED FEATURE HEURISTICS
// =============================================================================

struct GraphFeatures {
    std::vector<float> degree;
    std::vector<float> clustering;
    std::vector<float> betweenness_approx;
    std::vector<float> pagerank_approx;
    float max_degree;
    float avg_degree;
    float degree_variance;
    
    static GraphFeatures compute(const Graph& graph) {
        GraphFeatures f;
        const size_t n = graph.num_nodes();
        f.degree.resize(n);
        f.clustering.resize(n);
        f.betweenness_approx.resize(n, 0);
        f.pagerank_approx.resize(n, 1.0f / n);
        
        f.max_degree = 0;
        f.avg_degree = 0;
        
        // Compute degrees
        for (NodeId v = 0; v < n; ++v) {
            f.degree[v] = graph.out_degree(v);
            f.max_degree = std::max(f.max_degree, f.degree[v]);
            f.avg_degree += f.degree[v];
        }
        f.avg_degree /= n;
        
        // Compute degree variance
        f.degree_variance = 0;
        for (NodeId v = 0; v < n; ++v) {
            float diff = f.degree[v] - f.avg_degree;
            f.degree_variance += diff * diff;
        }
        f.degree_variance = std::sqrt(f.degree_variance / n);
        
        // Approximate clustering coefficient
        for (NodeId v = 0; v < n; ++v) {
            auto edges = graph.out_edges(v);
            if (edges.size() < 2) {
                f.clustering[v] = 0;
                continue;
            }
            
            // Count triangles (sample for efficiency)
            size_t triangles = 0;
            size_t possible = edges.size() * (edges.size() - 1) / 2;
            size_t sample_size = std::min(possible, size_t(20));
            
            std::set<NodeId> neighbors;
            for (const auto& e : edges) {
                neighbors.insert(e.target);
            }
            
            size_t checked = 0;
            for (size_t i = 0; i < edges.size() && checked < sample_size; ++i) {
                for (const auto& e2 : graph.out_edges(edges[i].target)) {
                    if (neighbors.count(e2.target) && e2.target != v) {
                        triangles++;
                    }
                    checked++;
                    if (checked >= sample_size) break;
                }
            }
            
            f.clustering[v] = (possible > 0) ? 
                static_cast<float>(triangles) / sample_size : 0;
        }
        
        // Approximate PageRank (5 iterations)
        std::vector<float> pr_new(n);
        float damping = 0.85f;
        for (int iter = 0; iter < 5; ++iter) {
            std::fill(pr_new.begin(), pr_new.end(), (1.0f - damping) / n);
            for (NodeId v = 0; v < n; ++v) {
                auto edges = graph.out_edges(v);
                if (edges.empty()) continue;
                float contrib = damping * f.pagerank_approx[v] / edges.size();
                for (const auto& e : edges) {
                    pr_new[e.target] += contrib;
                }
            }
            std::swap(f.pagerank_approx, pr_new);
        }
        
        // Normalize pagerank
        float pr_max = *std::max_element(f.pagerank_approx.begin(), f.pagerank_approx.end());
        if (pr_max > 0) {
            for (float& pr : f.pagerank_approx) pr /= pr_max;
        }
        
        // Approximate betweenness (via random sampling)
        std::mt19937 rng(42);
        std::uniform_int_distribution<NodeId> dist(0, n - 1);
        for (int sample = 0; sample < 50; ++sample) {
            NodeId s = dist(rng);
            // BFS from s, count how often each node is on shortest paths
            std::vector<int> depth(n, -1);
            std::vector<int> num_paths(n, 0);
            std::queue<NodeId> q;
            q.push(s);
            depth[s] = 0;
            num_paths[s] = 1;
            
            while (!q.empty()) {
                NodeId u = q.front();
                q.pop();
                for (const auto& e : graph.out_edges(u)) {
                    if (depth[e.target] == -1) {
                        depth[e.target] = depth[u] + 1;
                        num_paths[e.target] = num_paths[u];
                        q.push(e.target);
                    } else if (depth[e.target] == depth[u] + 1) {
                        num_paths[e.target] += num_paths[u];
                    }
                }
            }
            
            // Nodes with many paths through them have high betweenness
            for (NodeId v = 0; v < n; ++v) {
                if (depth[v] > 0) {
                    f.betweenness_approx[v] += num_paths[v];
                }
            }
        }
        
        // Normalize betweenness
        float bc_max = *std::max_element(f.betweenness_approx.begin(), f.betweenness_approx.end());
        if (bc_max > 0) {
            for (float& bc : f.betweenness_approx) bc /= bc_max;
        }
        
        return f;
    }
};

// =============================================================================
// ADVANCED HEURISTIC FORMULAS
// =============================================================================

// H1: Degree only (normalized)
struct H_DegreeNorm {
    const GraphFeatures& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * (f.degree[v] / f.max_degree);
    }
};

// H2: Log entropy (original)
struct H_LogEntropy {
    const GraphFeatures& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * entropy_log(f.degree[v]);
    }
};

// H3: Sqrt entropy
struct H_SqrtEntropy {
    const GraphFeatures& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * entropy_sqrt(f.degree[v]) / std::sqrt(f.max_degree);
    }
};

// H4: PageRank-guided
struct H_PageRank {
    const GraphFeatures& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * f.pagerank_approx[v];
    }
};

// H5: Betweenness-guided
struct H_Betweenness {
    const GraphFeatures& f;
    float weight;
    
    float operator()(NodeId v, float g) const {
        return g - weight * f.betweenness_approx[v];
    }
};

// H6: Combined: degree + PageRank
struct H_DegreePageRank {
    const GraphFeatures& f;
    float w_deg, w_pr;
    
    float operator()(NodeId v, float g) const {
        float deg_term = f.degree[v] / f.max_degree;
        float pr_term = f.pagerank_approx[v];
        return g - w_deg * deg_term - w_pr * pr_term;
    }
};

// H7: Combined: degree + betweenness
struct H_DegreeBetweenness {
    const GraphFeatures& f;
    float w_deg, w_bc;
    
    float operator()(NodeId v, float g) const {
        float deg_term = f.degree[v] / f.max_degree;
        float bc_term = f.betweenness_approx[v];
        return g - w_deg * deg_term - w_bc * bc_term;
    }
};

// H8: Distance-scaled entropy (entropy matters more for longer paths)
struct H_DistanceScaled {
    const GraphFeatures& f;
    float base_weight;
    
    float operator()(NodeId v, float g) const {
        // As distance grows, entropy matters more
        float scale = std::log2(g + 2.0f);
        return g - base_weight * scale * entropy_log(f.degree[v]) / 10.0f;
    }
};

// H9: Variance-adaptive (weight based on degree variance)
struct H_VarianceAdaptive {
    const GraphFeatures& f;
    
    float operator()(NodeId v, float g) const {
        // High variance = entropy matters more
        float weight = std::min(0.5f, f.degree_variance / f.avg_degree * 0.15f);
        return g - weight * entropy_log(f.degree[v]);
    }
};

// H10: Clustering-aware (prefer nodes with low clustering = bridges)
struct H_ClusteringAware {
    const GraphFeatures& f;
    float w_deg, w_clust;
    
    float operator()(NodeId v, float g) const {
        // High degree good, high clustering bad (want bridges)
        float deg_term = f.degree[v] / f.max_degree;
        float clust_term = f.clustering[v];
        return g - w_deg * deg_term + w_clust * clust_term;
    }
};

// H11: All-in-one: combines everything
struct H_AllFeatures {
    const GraphFeatures& f;
    float w_deg, w_pr, w_bc, w_clust;
    
    float operator()(NodeId v, float g) const {
        float score = 0;
        score += w_deg * (f.degree[v] / f.max_degree);
        score += w_pr * f.pagerank_approx[v];
        score += w_bc * f.betweenness_approx[v];
        score -= w_clust * f.clustering[v];  // Penalize high clustering
        return g - score;
    }
};

// H12: Sigmoid degree (saturating)
struct H_SigmoidDegree {
    const GraphFeatures& f;
    float weight;
    float k;  // saturation constant
    
    float operator()(NodeId v, float g) const {
        return g - weight * entropy_sigmoid(f.degree[v], k);
    }
};

} // namespace formulas

// =============================================================================
// EXPERIMENT RUNNER
// =============================================================================

struct FormulaResult {
    std::string name;
    double avg_speedup;
    double avg_optimality;
    double avg_node_reduction;
    bool always_optimal;
    int wins;  // number of graphs where it was fastest optimal
};

int main() {
    using namespace formulas;
    
    std::cout << R"(
╔══════════════════════════════════════════════════════════════════════════════════╗
║                         DEEP FORMULA EXPLORATION                                 ║
║                                                                                  ║
║  Testing 15+ formula variants across multiple graph types                        ║
║  Goal: Find the optimal heuristic formulation                                    ║
╚══════════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Generate diverse test graphs
    std::vector<std::pair<std::string, Graph>> test_graphs;
    test_graphs.push_back({"Social 3000", generators::social_network(3000, 12)});
    test_graphs.push_back({"Scale-Free 2500", generators::scale_free(2500, 2.2f)});
    test_graphs.push_back({"Road 2000", generators::road_network(2000, 7)});
    test_graphs.push_back({"Grid 50x50", generators::grid(50, 50, true, true)});
    test_graphs.push_back({"Maze 40x40", generators::maze(40, 40)});
    test_graphs.push_back({"Random Dense 1000", generators::random_graph(1000, 0.05f, true)});
    
    // Results accumulator
    std::map<std::string, FormulaResult> results;
    
    for (auto& [graph_name, graph] : test_graphs) {
        std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        std::cout << "📊 " << graph_name << " (" << graph.num_nodes() << " nodes, " 
                  << graph.num_edges() << " edges)\n";
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
        
        // Compute graph features
        auto features = GraphFeatures::compute(graph);
        std::cout << "  Features: max_deg=" << features.max_degree 
                  << " avg_deg=" << std::fixed << std::setprecision(1) << features.avg_degree
                  << " variance=" << features.degree_variance << "\n\n";
        
        // Generate queries
        const size_t num_queries = 40;
        std::mt19937 rng(42);
        std::uniform_int_distribution<NodeId> dist(0, graph.num_nodes() - 1);
        std::vector<std::pair<NodeId, NodeId>> queries;
        for (size_t i = 0; i < num_queries; ++i) {
            NodeId s = dist(rng), t = dist(rng);
            while (t == s) t = dist(rng);
            queries.push_back({s, t});
        }
        
        // Get Dijkstra baseline
        double dij_time = 0, dij_nodes = 0;
        std::vector<float> optimal_dist;
        for (auto [s, t] : queries) {
            auto r = search::dijkstra(graph, s, t);
            dij_time += r.time_microseconds;
            dij_nodes += r.nodes_explored;
            optimal_dist.push_back(r.distance);
        }
        dij_time /= queries.size();
        dij_nodes /= queries.size();
        
        // Best formula tracking for this graph
        std::string best_optimal_formula = "Dijkstra";
        double best_optimal_speedup = 1.0;
        
        // Test each formula
        auto test_formula = [&](const char* name, auto&& create_heuristic) {
            double total_time = 0, total_nodes = 0, total_ratio = 0;
            bool all_optimal = true;
            
            for (size_t i = 0; i < queries.size(); ++i) {
                auto [s, t] = queries[i];
                auto h = create_heuristic();
                auto r = priority_dijkstra(graph, s, t, h, name);
                
                total_time += r.time_microseconds;
                total_nodes += r.nodes_explored;
                
                if (r.found() && optimal_dist[i] < INF_DIST) {
                    double ratio = r.distance / optimal_dist[i];
                    total_ratio += ratio;
                    if (ratio > 1.001) all_optimal = false;
                }
            }
            
            double avg_time = total_time / queries.size();
            double speedup = dij_time / avg_time;
            double avg_nodes = total_nodes / queries.size();
            double node_reduction = (dij_nodes - avg_nodes) / dij_nodes * 100;
            double avg_ratio = total_ratio / queries.size();
            
            // Update results
            if (results.find(name) == results.end()) {
                results[name] = {name, 0, 0, 0, true, 0};
            }
            results[name].avg_speedup += speedup;
            results[name].avg_optimality += avg_ratio;
            results[name].avg_node_reduction += node_reduction;
            if (!all_optimal) results[name].always_optimal = false;
            
            // Track best for this graph
            if (all_optimal && speedup > best_optimal_speedup) {
                best_optimal_speedup = speedup;
                best_optimal_formula = name;
            }
            
            // Print result
            std::cout << std::left << std::setw(25) << name 
                      << " │ " << std::right << std::setw(6) << std::fixed << std::setprecision(2) 
                      << speedup << "× │ " 
                      << std::setw(5) << std::setprecision(3) << avg_ratio << " │ "
                      << (all_optimal ? "✓" : "✗") << " │ "
                      << std::showpos << std::setw(5) << std::setprecision(0) << node_reduction << "%" 
                      << std::noshowpos << "\n";
        };
        
        std::cout << std::left << std::setw(25) << "Formula" 
                  << " │ Speedup │ Ratio │ Opt │ Nodes\n";
        std::cout << "─────────────────────────────────────────────────────────────────────\n";
        std::cout << std::left << std::setw(25) << "Dijkstra (baseline)" 
                  << " │   1.00× │ 1.000 │ ✓ │   +0%\n";
        
        // Test all formula variants
        
        // Weight variations for degree-based formulas
        for (float w : {0.05f, 0.1f, 0.2f, 0.3f, 0.5f}) {
            std::string name = "Degree-Norm w=" + std::to_string(w).substr(0,4);
            test_formula(name.c_str(), [&]() { 
                return H_DegreeNorm{features, w}; 
            });
        }
        
        // Log entropy variations
        for (float w : {0.1f, 0.2f, 0.3f}) {
            std::string name = "Log-Entropy w=" + std::to_string(w).substr(0,4);
            test_formula(name.c_str(), [&]() { 
                return H_LogEntropy{features, w}; 
            });
        }
        
        // Sqrt entropy
        test_formula("Sqrt-Entropy w=0.2", [&]() { 
            return H_SqrtEntropy{features, 0.2f}; 
        });
        
        // PageRank-guided
        test_formula("PageRank w=0.3", [&]() { 
            return H_PageRank{features, 0.3f}; 
        });
        
        // Betweenness-guided
        test_formula("Betweenness w=0.3", [&]() { 
            return H_Betweenness{features, 0.3f}; 
        });
        
        // Combined: degree + PageRank
        test_formula("Deg+PR (0.15,0.15)", [&]() { 
            return H_DegreePageRank{features, 0.15f, 0.15f}; 
        });
        
        // Combined: degree + betweenness
        test_formula("Deg+BC (0.15,0.15)", [&]() { 
            return H_DegreeBetweenness{features, 0.15f, 0.15f}; 
        });
        
        // Distance-scaled
        test_formula("Dist-Scaled w=0.1", [&]() { 
            return H_DistanceScaled{features, 0.1f}; 
        });
        
        // Variance-adaptive
        test_formula("Variance-Adaptive", [&]() { 
            return H_VarianceAdaptive{features}; 
        });
        
        // Clustering-aware
        test_formula("Cluster-Aware", [&]() { 
            return H_ClusteringAware{features, 0.2f, 0.1f}; 
        });
        
        // All features combined
        test_formula("All-Features", [&]() { 
            return H_AllFeatures{features, 0.1f, 0.1f, 0.1f, 0.05f}; 
        });
        
        // Sigmoid variations
        for (float k : {5.0f, 10.0f, 20.0f}) {
            std::string name = "Sigmoid k=" + std::to_string(int(k));
            test_formula(name.c_str(), [&, k]() { 
                return H_SigmoidDegree{features, 0.3f, k}; 
            });
        }
        
        std::cout << "─────────────────────────────────────────────────────────────────────\n";
        std::cout << "★ Best optimal formula: " << best_optimal_formula 
                  << " (" << std::setprecision(2) << best_optimal_speedup << "× speedup)\n";
        
        // Record win
        if (best_optimal_formula != "Dijkstra") {
            results[best_optimal_formula].wins++;
        }
    }
    
    // Final rankings
    std::cout << "\n\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                              FINAL RANKINGS                                      ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ Formula                    │ Avg Speedup │ Avg Opt │ Always Opt │ Graph Wins    ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════╣\n";
    
    // Sort by average speedup among always-optimal formulas
    std::vector<std::pair<std::string, FormulaResult>> sorted_results(results.begin(), results.end());
    std::sort(sorted_results.begin(), sorted_results.end(), [&](auto& a, auto& b) {
        // Prefer always-optimal, then by speedup
        if (a.second.always_optimal != b.second.always_optimal) {
            return a.second.always_optimal > b.second.always_optimal;
        }
        return a.second.avg_speedup > b.second.avg_speedup;
    });
    
    size_t num_graphs = test_graphs.size();
    for (auto& [name, r] : sorted_results) {
        std::cout << "║ " << std::left << std::setw(26) << name << " │ "
                  << std::right << std::setw(10) << std::fixed << std::setprecision(2) 
                  << (r.avg_speedup / num_graphs) << "× │ "
                  << std::setw(7) << std::setprecision(4) << (r.avg_optimality / num_graphs) << " │ "
                  << (r.always_optimal ? "     ✓     " : "     ✗     ") << " │ "
                  << std::setw(5) << r.wins << " wins    ║\n";
    }
    
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════╝\n";
    
    // Find the champion
    std::string champion = "";
    double champion_speedup = 0;
    for (auto& [name, r] : sorted_results) {
        if (r.always_optimal && r.avg_speedup / num_graphs > champion_speedup) {
            champion = name;
            champion_speedup = r.avg_speedup / num_graphs;
        }
    }
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                              CHAMPION FORMULA                                    ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║                                                                                  ║\n";
    std::cout << "║  🏆 " << std::left << std::setw(72) << champion << "   ║\n";
    std::cout << "║                                                                                  ║\n";
    std::cout << "║  Average Speedup: " << std::right << std::setw(5) << std::setprecision(2) 
              << champion_speedup << "×                                                           ║\n";
    std::cout << "║  Always Optimal:  ✓                                                              ║\n";
    std::cout << "║                                                                                  ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════╝\n";
    
    return 0;
}
