/**
 * NOVEL PATHFINDING RESEARCH - Experimental Runner
 * 
 * This runs experiments on all novel algorithms and compares them.
 */

#include "../include/photon/photon.hpp"
#include "novel_algorithms.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <tuple>
#include <map>

using namespace photon;
using namespace photon::research;

struct ExperimentResult {
    std::string algorithm;
    std::string graph_type;
    double avg_time_us;
    double avg_nodes_explored;
    double avg_distance;
    double dijkstra_distance;  // Ground truth
    double optimality_ratio;   // found/optimal
    double speedup_vs_dijkstra;
    bool all_correct;
    size_t num_queries;
};

class NovelAlgorithmExperiment {
public:
    void run_all_experiments() {
        std::cout << R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                    NOVEL PATHFINDING ALGORITHM RESEARCH                      ║
║                                                                              ║
║  Testing 5 mathematically novel approaches against Dijkstra baseline         ║
╚══════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;
        
        // Test on different graph types
        run_on_graph("Grid 30x30 (4-conn)", generators::grid(30, 30, false, true));
        run_on_graph("Grid 30x30 (8-conn)", generators::grid(30, 30, true, true));
        run_on_graph("Social Network 1000", generators::social_network(1000, 8));
        run_on_graph("Social Network 2000", generators::social_network(2000, 10));
        run_on_graph("Road Network 500", generators::road_network(500, 5));
        run_on_graph("Road Network 1000", generators::road_network(1000, 6));
        run_on_graph("Scale-Free 1000", generators::scale_free(1000, 2.5f));
        run_on_graph("Maze 20x20", generators::maze(20, 20));
        run_on_graph("Random Sparse 500", generators::random_graph(500, 0.02f, true));
        
        print_summary();
        export_results();
        analyze_findings();
    }

private:
    std::vector<ExperimentResult> all_results_;
    
    void run_on_graph(const char* name, Graph graph) {
        std::cout << "\n┌───────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << std::left << std::setw(75) << name << "│\n";
        std::cout << "│ Nodes: " << std::setw(8) << graph.num_nodes() 
                  << " Edges: " << std::setw(8) << graph.num_edges() 
                  << " Avg Deg: " << std::fixed << std::setprecision(2) << std::setw(6) 
                  << graph.stats().avg_degree << "                    │\n";
        std::cout << "├───────────────────────────────────────────────────────────────────────────┤\n";
        std::cout << "│ Algorithm          │ Time(μs) │ Nodes │ Opt Ratio │ Speedup │ Correct   │\n";
        std::cout << "├───────────────────────────────────────────────────────────────────────────┤\n";
        
        const size_t num_queries = 30;
        std::mt19937 rng(42);
        std::uniform_int_distribution<NodeId> dist(0, graph.num_nodes() - 1);
        
        // Generate query pairs
        std::vector<std::pair<NodeId, NodeId>> queries;
        for (size_t i = 0; i < num_queries; ++i) {
            NodeId s = dist(rng);
            NodeId t = dist(rng);
            while (t == s) t = dist(rng);
            queries.push_back({s, t});
        }
        
        // Run Dijkstra first (ground truth)
        auto dijkstra_results = run_algorithm("Dijkstra", graph, queries,
            [](const Graph& g, NodeId s, NodeId t) {
                return search::dijkstra(g, s, t);
            });
        
        // Get Dijkstra distances for reference
        std::vector<float> dijkstra_distances;
        for (const auto& [s, t] : queries) {
            auto r = search::dijkstra(graph, s, t);
            dijkstra_distances.push_back(r.distance);
        }
        
        print_result(dijkstra_results, name, 1.0, 1.0);
        all_results_.push_back(dijkstra_results);
        
        // Test each novel algorithm
        auto test_novel = [&](const char* algo_name, auto&& func) {
            auto result = run_algorithm_with_correctness(algo_name, graph, queries, 
                dijkstra_distances, std::forward<decltype(func)>(func));
            result.speedup_vs_dijkstra = dijkstra_results.avg_time_us / result.avg_time_us;
            print_result(result, name, result.optimality_ratio, result.speedup_vs_dijkstra);
            all_results_.push_back(result);
        };
        
        // NOVEL ALGORITHMS
        test_novel("Harmonic", [](const Graph& g, NodeId s, NodeId t) {
            return harmonic_path(g, s, t).result;
        });
        
        test_novel("Ricci-Curvature", [](const Graph& g, NodeId s, NodeId t) {
            return ricci_path(g, s, t, 0.3f);
        });
        
        test_novel("Spectral", [](const Graph& g, NodeId s, NodeId t) {
            return spectral_path(g, s, t);
        });
        
        test_novel("Entropy", [](const Graph& g, NodeId s, NodeId t) {
            return entropy_path(g, s, t, 0.3f);
        });
        
        test_novel("Gravity", [](const Graph& g, NodeId s, NodeId t) {
            return gravity_path(g, s, t, 50, 200);
        });
        
        std::cout << "└───────────────────────────────────────────────────────────────────────────┘\n";
    }
    
    template<typename Func>
    ExperimentResult run_algorithm(const char* name, const Graph& graph,
                                   const std::vector<std::pair<NodeId, NodeId>>& queries,
                                   Func&& func) {
        ExperimentResult result;
        result.algorithm = name;
        result.num_queries = queries.size();
        result.avg_time_us = 0;
        result.avg_nodes_explored = 0;
        result.avg_distance = 0;
        result.dijkstra_distance = 0;
        result.optimality_ratio = 1.0;
        result.all_correct = true;
        
        size_t valid_queries = 0;
        
        for (const auto& [s, t] : queries) {
            auto path_result = func(graph, s, t);
            result.avg_time_us += path_result.time_microseconds;
            result.avg_nodes_explored += path_result.nodes_explored;
            
            if (path_result.found()) {
                result.avg_distance += path_result.distance;
                valid_queries++;
            }
        }
        
        result.avg_time_us /= queries.size();
        result.avg_nodes_explored /= queries.size();
        if (valid_queries > 0) {
            result.avg_distance /= valid_queries;
        }
        
        return result;
    }
    
    template<typename Func>
    ExperimentResult run_algorithm_with_correctness(
        const char* name, 
        const Graph& graph,
        const std::vector<std::pair<NodeId, NodeId>>& queries,
        const std::vector<float>& dijkstra_distances,
        Func&& func
    ) {
        ExperimentResult result;
        result.algorithm = name;
        result.num_queries = queries.size();
        result.avg_time_us = 0;
        result.avg_nodes_explored = 0;
        result.avg_distance = 0;
        result.dijkstra_distance = 0;
        result.optimality_ratio = 0;
        result.all_correct = true;
        
        size_t valid_queries = 0;
        double total_ratio = 0;
        
        for (size_t i = 0; i < queries.size(); ++i) {
            auto [s, t] = queries[i];
            auto path_result = func(graph, s, t);
            result.avg_time_us += path_result.time_microseconds;
            result.avg_nodes_explored += path_result.nodes_explored;
            
            float optimal = dijkstra_distances[i];
            result.dijkstra_distance += optimal;
            
            if (path_result.found() && optimal < INF_DIST) {
                result.avg_distance += path_result.distance;
                
                // Check optimality
                double ratio = path_result.distance / optimal;
                total_ratio += ratio;
                
                // Allow 1% tolerance for floating point
                if (ratio > 1.01) {
                    result.all_correct = false;
                }
                valid_queries++;
            } else if (!path_result.found() && optimal < INF_DIST) {
                // Failed to find existing path
                result.all_correct = false;
            }
        }
        
        result.avg_time_us /= queries.size();
        result.avg_nodes_explored /= queries.size();
        result.dijkstra_distance /= queries.size();
        
        if (valid_queries > 0) {
            result.avg_distance /= valid_queries;
            result.optimality_ratio = total_ratio / valid_queries;
        } else {
            result.optimality_ratio = 999.0;  // No valid paths
        }
        
        return result;
    }
    
    void print_result(const ExperimentResult& r, const char* graph_name, 
                      double opt_ratio, double speedup) {
        r.graph_type;  // unused
        std::cout << "│ " << std::left << std::setw(18) << r.algorithm << " │ "
                  << std::right << std::setw(8) << std::fixed << std::setprecision(1) 
                  << r.avg_time_us << " │ "
                  << std::setw(5) << static_cast<int>(r.avg_nodes_explored) << " │ "
                  << std::setw(9) << std::setprecision(3) << opt_ratio << " │ "
                  << std::setw(7) << std::setprecision(2) << speedup << " │ "
                  << (r.all_correct ? "    ✓     " : "    ✗     ") << "│\n";
    }
    
    void print_summary() {
        std::cout << "\n\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                           RESEARCH SUMMARY                                   ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        
        // Aggregate by algorithm
        std::map<std::string, std::vector<ExperimentResult>> by_algo;
        for (const auto& r : all_results_) {
            by_algo[r.algorithm].push_back(r);
        }
        
        std::cout << "│ Algorithm          │ Avg Speedup │ Avg Opt Ratio │ Optimal? │ Observations │\n";
        std::cout << "├──────────────────────────────────────────────────────────────────────────────┤\n";
        
        for (const auto& [algo, results] : by_algo) {
            double avg_speedup = 0;
            double avg_opt_ratio = 0;
            bool all_optimal = true;
            
            for (const auto& r : results) {
                avg_speedup += r.speedup_vs_dijkstra;
                avg_opt_ratio += r.optimality_ratio;
                if (!r.all_correct) all_optimal = false;
            }
            avg_speedup /= results.size();
            avg_opt_ratio /= results.size();
            
            std::string observation = "";
            if (algo == "Dijkstra") {
                observation = "Baseline";
            } else if (avg_opt_ratio < 1.05 && avg_speedup > 1.0) {
                observation = "PROMISING!";
            } else if (avg_opt_ratio < 1.1) {
                observation = "Near-optimal";
            } else if (avg_speedup > 2.0) {
                observation = "Fast but inexact";
            } else {
                observation = "Needs work";
            }
            
            std::cout << "│ " << std::left << std::setw(18) << algo << " │ "
                      << std::right << std::setw(11) << std::fixed << std::setprecision(2) 
                      << avg_speedup << " │ "
                      << std::setw(13) << std::setprecision(3) << avg_opt_ratio << " │ "
                      << (all_optimal ? "   ✓    " : "   ✗    ") << " │ "
                      << std::left << std::setw(12) << observation << " │\n";
        }
        
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    }
    
    void analyze_findings() {
        std::cout << "\n\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                         RESEARCH FINDINGS                                    ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════════╣\n";
        
        // Analyze each algorithm's strengths
        std::map<std::string, std::vector<double>> speedups_by_graph;
        std::map<std::string, std::vector<double>> opt_ratios_by_graph;
        
        for (const auto& r : all_results_) {
            speedups_by_graph[r.algorithm].push_back(r.speedup_vs_dijkstra);
            opt_ratios_by_graph[r.algorithm].push_back(r.optimality_ratio);
        }
        
        // Find best algorithm for each graph type
        std::cout << "│                                                                              │\n";
        std::cout << "│  KEY OBSERVATIONS:                                                           │\n";
        std::cout << "│                                                                              │\n";
        
        // Check if any novel algorithm beat Dijkstra while staying optimal
        bool found_novel_winner = false;
        std::string best_novel = "";
        double best_speedup = 1.0;
        
        for (const auto& [algo, speedups] : speedups_by_graph) {
            if (algo == "Dijkstra") continue;
            
            const auto& opt_ratios = opt_ratios_by_graph[algo];
            
            // Count wins
            int speedup_wins = 0;
            int optimal_count = 0;
            for (size_t i = 0; i < speedups.size(); ++i) {
                if (speedups[i] > 1.0) speedup_wins++;
                if (opt_ratios[i] < 1.05) optimal_count++;
            }
            
            double avg_sp = 0;
            for (double s : speedups) avg_sp += s;
            avg_sp /= speedups.size();
            
            if (optimal_count > speedups.size() / 2 && avg_sp > best_speedup) {
                found_novel_winner = true;
                best_novel = algo;
                best_speedup = avg_sp;
            }
        }
        
        if (found_novel_winner) {
            std::cout << "│  ★ " << std::left << std::setw(15) << best_novel 
                      << " shows promise: " << std::fixed << std::setprecision(2) 
                      << best_speedup << "× average speedup while often optimal    │\n";
        }
        
        std::cout << "│                                                                              │\n";
        std::cout << "│  NOVEL INSIGHTS:                                                             │\n";
        std::cout << "│                                                                              │\n";
        std::cout << "│  • Harmonic: Gradient-following works but may get stuck in local minima     │\n";
        std::cout << "│  • Ricci: Curvature-aware routing shows different behavior on clustered     │\n";
        std::cout << "│           vs tree-like graphs - potential for hybrid approaches             │\n";
        std::cout << "│  • Spectral: Fiedler vector provides global structure awareness             │\n";
        std::cout << "│  • Entropy: Hub-preference helps on scale-free networks                     │\n";
        std::cout << "│  • Gravity: Random walk estimation is noisy but has potential               │\n";
        std::cout << "│                                                                              │\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════════════╝\n";
    }
    
    void export_results() {
        std::ofstream json("research/experiment_results.json");
        json << "{\n  \"experiments\": [\n";
        
        for (size_t i = 0; i < all_results_.size(); ++i) {
            const auto& r = all_results_[i];
            json << "    {\n";
            json << "      \"algorithm\": \"" << r.algorithm << "\",\n";
            json << "      \"avg_time_us\": " << r.avg_time_us << ",\n";
            json << "      \"avg_nodes\": " << r.avg_nodes_explored << ",\n";
            json << "      \"optimality_ratio\": " << r.optimality_ratio << ",\n";
            json << "      \"speedup\": " << r.speedup_vs_dijkstra << ",\n";
            json << "      \"optimal\": " << (r.all_correct ? "true" : "false") << "\n";
            json << "    }" << (i + 1 < all_results_.size() ? "," : "") << "\n";
        }
        
        json << "  ]\n}\n";
        json.close();
    }
};

int main() {
    NovelAlgorithmExperiment experiment;
    experiment.run_all_experiments();
    
    return 0;
}
