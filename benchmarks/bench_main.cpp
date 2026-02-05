/**
 * PHOTON Comprehensive Benchmark Suite
 */

#include "photon/photon.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#include <map>

using namespace photon;

struct BenchmarkResult {
    std::string graph_name;
    std::string algorithm;
    size_t num_queries;
    double total_time_us;
    double avg_time_us;
    uint64_t total_nodes_explored;
    uint64_t total_edges_relaxed;
    double avg_nodes_per_query;
    double speedup_vs_dijkstra;
    bool all_correct;
};

class Benchmark {
public:
    void add_graph(const char* name, Graph graph) {
        graphs_.push_back({name, std::move(graph)});
    }
    
    void run(size_t num_queries = 50, size_t seed = 42) {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                    PHOTON COMPREHENSIVE BENCHMARK                        ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════╣\n";
        std::cout << "║ Queries per graph: " << std::setw(5) << num_queries 
                  << "                                                    ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════════════════════╝\n";
        
        for (auto& [name, graph] : graphs_) {
            run_on_graph(name, graph, num_queries, seed);
        }
        
        print_summary();
        export_results();
    }

private:
    std::vector<std::pair<std::string, Graph>> graphs_;
    std::vector<BenchmarkResult> results_;
    
    void run_on_graph(const std::string& name, const Graph& graph, 
                      size_t num_queries, size_t seed) {
        std::cout << "\n┌──────────────────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ " << std::left << std::setw(74) << name << "│\n";
        std::cout << "│ Nodes: " << std::setw(10) << graph.num_nodes() 
                  << " Edges: " << std::setw(10) << graph.num_edges()
                  << " Avg Degree: " << std::setw(8) << std::fixed << std::setprecision(2) 
                  << graph.stats().avg_degree << "          │\n";
        std::cout << "├──────────────────────────────────────────────────────────────────────────┤\n";
        std::cout << "│ Algorithm              │   Avg Time │  Nodes Exp │  Speedup │ Correct │\n";
        std::cout << "├──────────────────────────────────────────────────────────────────────────┤\n";
        
        // Generate random queries
        std::mt19937 rng(seed);
        std::uniform_int_distribution<NodeId> dist(0, graph.num_nodes() - 1);
        
        std::vector<std::pair<NodeId, NodeId>> queries(num_queries);
        for (auto& [s, t] : queries) {
            s = dist(rng);
            t = dist(rng);
            while (t == s) t = dist(rng);
        }
        
        // Run Dijkstra first (baseline)
        BenchmarkResult dijkstra_result = run_algorithm(
            "Dijkstra", graph, queries,
            [](const Graph& g, NodeId s, NodeId t) { 
                return search::dijkstra(g, s, t); 
            }
        );
        dijkstra_result.graph_name = name;
        dijkstra_result.speedup_vs_dijkstra = 1.0;
        print_result(dijkstra_result);
        results_.push_back(dijkstra_result);
        
        // Other algorithms
        auto run_and_record = [&](const char* algo_name, auto&& func) {
            auto result = run_algorithm(algo_name, graph, queries, 
                std::forward<decltype(func)>(func));
            result.graph_name = name;
            result.speedup_vs_dijkstra = dijkstra_result.avg_time_us / result.avg_time_us;
            print_result(result);
            results_.push_back(result);
        };
        
        run_and_record("Bidirectional SIMD", 
            [](const Graph& g, NodeId s, NodeId t) { 
                return search::bidirectional_simd(g, s, t); 
            });
        
        run_and_record("A*", 
            [](const Graph& g, NodeId s, NodeId t) { 
                return search::a_star(g, s, t); 
            });
        
        run_and_record("Delta-Stepping", 
            [](const Graph& g, NodeId s, NodeId t) { 
                return search::delta_stepping(g, s, t); 
            });
        
        run_and_record("Parallel Wavefront", 
            [](const Graph& g, NodeId s, NodeId t) { 
                return search::parallel_wavefront(g, s, t); 
            });
        
        // PHOTON Auto
        run_and_record("PHOTON (Auto)", 
            [](const Graph& g, NodeId s, NodeId t) {
                return shortest_path(g, s, t, Strategy::AUTO);
            });
        
        std::cout << "└──────────────────────────────────────────────────────────────────────────┘\n";
    }
    
    template<typename Func>
    BenchmarkResult run_algorithm(const char* name, const Graph& graph,
                                 const std::vector<std::pair<NodeId, NodeId>>& queries,
                                 Func&& func) {
        BenchmarkResult result;
        result.algorithm = name;
        result.num_queries = queries.size();
        result.total_time_us = 0;
        result.total_nodes_explored = 0;
        result.total_edges_relaxed = 0;
        result.all_correct = true;
        
        // Get Dijkstra results for verification
        std::vector<Distance> expected;
        if (std::string(name) != "Dijkstra") {
            for (const auto& [s, t] : queries) {
                auto dij = search::dijkstra(graph, s, t);
                expected.push_back(dij.distance);
            }
        }
        
        for (size_t i = 0; i < queries.size(); ++i) {
            auto [s, t] = queries[i];
            auto path_result = func(graph, s, t);
            
            result.total_time_us += path_result.time_microseconds;
            result.total_nodes_explored += path_result.nodes_explored;
            result.total_edges_relaxed += path_result.edges_relaxed;
            
            // Verify correctness
            if (!expected.empty()) {
                if (std::abs(path_result.distance - expected[i]) > 1e-4) {
                    result.all_correct = false;
                }
            }
        }
        
        result.avg_time_us = result.total_time_us / queries.size();
        result.avg_nodes_per_query = static_cast<double>(result.total_nodes_explored) / queries.size();
        
        return result;
    }
    
    void print_result(const BenchmarkResult& r) {
        std::cout << "│ " << std::left << std::setw(22) << r.algorithm << " │ "
                  << std::right << std::setw(8) << std::fixed << std::setprecision(1) 
                  << r.avg_time_us << " μs │ "
                  << std::setw(10) << static_cast<uint64_t>(r.avg_nodes_per_query) << " │ "
                  << std::setw(7) << std::setprecision(2) << r.speedup_vs_dijkstra << "× │ "
                  << (r.all_correct ? "  ✓    " : "  ✗    ") << "│\n";
    }
    
    void print_summary() {
        std::cout << "\n";
        std::cout << "╔══════════════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                              SUMMARY                                     ║\n";
        std::cout << "╠══════════════════════════════════════════════════════════════════════════╣\n";
        
        // Group by algorithm and compute average speedup
        std::map<std::string, std::vector<double>> speedups;
        for (const auto& r : results_) {
            speedups[r.algorithm].push_back(r.speedup_vs_dijkstra);
        }
        
        std::cout << "│ Algorithm              │  Average Speedup  │  Max Speedup  │            │\n";
        std::cout << "├──────────────────────────────────────────────────────────────────────────┤\n";
        
        for (const auto& [algo, speeds] : speedups) {
            double avg = 0;
            double max_sp = 0;
            for (double s : speeds) {
                avg += s;
                max_sp = std::max(max_sp, s);
            }
            avg /= speeds.size();
            
            std::cout << "│ " << std::left << std::setw(22) << algo << " │ "
                      << std::right << std::setw(15) << std::fixed << std::setprecision(2) 
                      << avg << "× │ "
                      << std::setw(11) << max_sp << "× │            │\n";
        }
        
        std::cout << "╚══════════════════════════════════════════════════════════════════════════╝\n";
    }
    
    void export_results() {
        std::ofstream json("benchmark_results.json");
        json << "{\n  \"results\": [\n";
        
        for (size_t i = 0; i < results_.size(); ++i) {
            const auto& r = results_[i];
            json << "    {\n";
            json << "      \"graph\": \"" << r.graph_name << "\",\n";
            json << "      \"algorithm\": \"" << r.algorithm << "\",\n";
            json << "      \"avg_time_us\": " << r.avg_time_us << ",\n";
            json << "      \"avg_nodes\": " << r.avg_nodes_per_query << ",\n";
            json << "      \"speedup\": " << r.speedup_vs_dijkstra << ",\n";
            json << "      \"correct\": " << (r.all_correct ? "true" : "false") << "\n";
            json << "    }" << (i + 1 < results_.size() ? "," : "") << "\n";
        }
        
        json << "  ]\n}\n";
        json.close();
        
        std::cout << "\nResults exported to benchmark_results.json\n";
    }
};

int main() {
    Benchmark bench;
    
    // Add various graph types
    bench.add_graph("Grid 50x50 (8-connected)", generators::grid(50, 50, true, true));
    bench.add_graph("Grid 100x100 (4-connected)", generators::grid(100, 100, false, true));
    bench.add_graph("Social Network (2000 nodes)", generators::social_network(2000, 8));
    bench.add_graph("Social Network (5000 nodes)", generators::social_network(5000, 10));
    bench.add_graph("Road Network (1000 nodes)", generators::road_network(1000, 5));
    bench.add_graph("Road Network (3000 nodes)", generators::road_network(3000, 6));
    bench.add_graph("Perfect Maze 30x30", generators::maze(30, 30));
    bench.add_graph("Perfect Maze 50x50", generators::maze(50, 50));
    bench.add_graph("Scale-Free (2000 nodes)", generators::scale_free(2000, 2.5f));
    bench.add_graph("Random Sparse (1000 nodes)", generators::random_graph(1000, 0.01f, true));
    
    bench.run(50);  // 50 queries per graph
    
    return 0;
}
