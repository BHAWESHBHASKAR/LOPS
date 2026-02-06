/**
 * Real-World SNAP Benchmark
 * Runs major algorithms on real SNAP datasets.
 */

#include "photon/dash.hpp"
#include "../include/photon/lops.hpp"
#include "../include/photon/search.hpp"
#include "../include/photon/engine.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace photon;

// =============================================================================
// SNAP EDGE LIST LOADER
// =============================================================================

class SNAPLoader {
public:
    static Graph load_edge_list(const std::string& filename, bool undirected = true) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        std::map<uint32_t, NodeId> node_map;
        std::vector<std::pair<NodeId, NodeId>> edges;
        std::string line;

        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            uint32_t src, dst;
            if (iss >> src >> dst) {
                if (node_map.find(src) == node_map.end()) {
                    node_map[src] = node_map.size();
                }
                if (node_map.find(dst) == node_map.end()) {
                    node_map[dst] = node_map.size();
                }

                edges.push_back({node_map[src], node_map[dst]});
                if (undirected) {
                    edges.push_back({node_map[dst], node_map[src]});
                }
            }
        }

        size_t n = node_map.size();
        GraphBuilder builder(n);
        for (const auto& [src, dst] : edges) {
            builder.add_edge(src, dst, 1.0f);
        }
        return builder.build();
    }

    static void print_graph_stats(const Graph& graph, const std::string& name) {
        size_t n = graph.num_nodes();
        size_t m = graph.num_edges();

        std::vector<size_t> degrees(n);
        double sum = 0, sum_sq = 0;
        size_t max_deg = 0;

        for (NodeId v = 0; v < n; ++v) {
            degrees[v] = graph.out_degree(v);
            max_deg = std::max(max_deg, degrees[v]);
            sum += degrees[v];
            sum_sq += degrees[v] * degrees[v];
        }

        double avg = sum / n;
        double variance = (sum_sq / n) - (avg * avg);
        if (variance < 0.0) variance = 0.0;
        double cv = std::sqrt(variance) / (avg + 1e-9);

        std::sort(degrees.rbegin(), degrees.rend());
        size_t top1_count = std::max(1UL, n / 100);
        size_t top1_sum = 0;
        for (size_t i = 0; i < top1_count; ++i) top1_sum += degrees[i];
        bool is_scale_free = (top1_sum > sum * 0.2) && cv > 0.8;

        std::cout << "\n  ┌─────────────────────────────────────────────────────────────┐\n";
        std::cout << "  │ 📊 " << std::left << std::setw(54) << name << " │\n";
        std::cout << "  ├─────────────────────────────────────────────────────────────┤\n";
        std::cout << "  │ Nodes:           " << std::setw(10) << n << "                              │\n";
        std::cout << "  │ Edges:           " << std::setw(10) << m << "                              │\n";
        std::cout << "  │ Avg Degree:      " << std::setw(10) << std::fixed << std::setprecision(1) << avg << "                              │\n";
        std::cout << "  │ Max Degree:      " << std::setw(10) << max_deg << "                              │\n";
        std::cout << "  │ CV:              " << std::setw(10) << std::setprecision(2) << cv << "                              │\n";
        std::cout << "  │ Scale-Free:      " << std::setw(10) << (is_scale_free ? "Yes ★" : "No") << "                              │\n";
        std::cout << "  └─────────────────────────────────────────────────────────────┘\n\n";
    }
};

// =============================================================================
// BENCHMARK RUNNER
// =============================================================================

struct AlgoResult {
    std::string algorithm;
    double avg_time_us = 0.0;
    double speedup = 1.0;
    double optimality_pct = 0.0;
    double avg_nodes = 0.0;
};

struct DatasetResult {
    std::string dataset_name;
    std::vector<AlgoResult> results;
};

static std::vector<std::pair<NodeId, NodeId>> make_queries(const Graph& graph, size_t count, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<NodeId> dist(0, static_cast<NodeId>(graph.num_nodes() - 1));
    std::vector<std::pair<NodeId, NodeId>> queries;
    queries.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        NodeId s = dist(rng), t = dist(rng);
        while (t == s) t = dist(rng);
        queries.push_back({s, t});
    }
    return queries;
}

static DatasetResult run_benchmark(const Graph& graph, size_t num_queries) {
    DatasetResult dataset_result;

    // Preprocess
    dash::DASH dash_solver;
    dash_solver.preprocess(graph);

    lops::LOPS lops_solver;
    lops_solver.preprocess(graph);

    // Queries
    auto queries = make_queries(graph, num_queries, 42);

    // Baseline Dijkstra
    double dij_time = 0.0;
    double dij_nodes = 0.0;
    std::vector<float> optimal_dists;
    optimal_dists.reserve(queries.size());

    for (auto [s, t] : queries) {
        auto r = search::dijkstra(graph, s, t);
        dij_time += r.time_microseconds;
        dij_nodes += r.nodes_explored;
        optimal_dists.push_back(r.distance);
    }

    auto add_result = [&](const std::string& name, double total_time, double total_nodes, int optimal) {
        AlgoResult r;
        r.algorithm = name;
        r.avg_time_us = total_time / static_cast<double>(num_queries);
        r.speedup = (dij_time / num_queries) / r.avg_time_us;
        r.optimality_pct = optimal * 100.0 / static_cast<double>(num_queries);
        r.avg_nodes = total_nodes / static_cast<double>(num_queries);
        dataset_result.results.push_back(r);
    };

    add_result("Dijkstra", dij_time, dij_nodes, static_cast<int>(num_queries));

    auto benchmark_algo = [&](const std::string& name, auto&& func) {
        double total_time = 0.0;
        double total_nodes = 0.0;
        int optimal = 0;

        for (size_t i = 0; i < queries.size(); ++i) {
            auto [s, t] = queries[i];
            auto r = func(s, t);
            total_time += r.time_microseconds;
            total_nodes += r.nodes_explored;

            if (optimal_dists[i] < INF_DIST) {
                if (r.found() && r.distance <= optimal_dists[i] * 1.001f) {
                    optimal++;
                }
            } else {
                if (!r.found() || r.distance >= INF_DIST) {
                    optimal++;
                }
            }
        }

        add_result(name, total_time, total_nodes, optimal);
    };

    benchmark_algo("Bidirectional SIMD", [&](NodeId s, NodeId t) {
        return search::bidirectional_simd(graph, s, t);
    });

    benchmark_algo("A*", [&](NodeId s, NodeId t) {
        return search::a_star(graph, s, t);
    });

    benchmark_algo("Delta-Stepping", [&](NodeId s, NodeId t) {
        return search::delta_stepping(graph, s, t);
    });

    benchmark_algo("Parallel Wavefront", [&](NodeId s, NodeId t) {
        return search::parallel_wavefront(graph, s, t);
    });

    benchmark_algo("LIPS-Exact", [&](NodeId s, NodeId t) {
        return lops_solver.query_exact(s, t);
    });

    benchmark_algo("LIPS-Weighted", [&](NodeId s, NodeId t) {
        return lops_solver.query_approx(s, t, 1.3f);
    });

    benchmark_algo("LIPS-Hybrid", [&](NodeId s, NodeId t) {
        return lops_solver.query_hybrid(s, t, 1.3f);
    });

    benchmark_algo("DASH-Single", [&](NodeId s, NodeId t) {
        return dash_solver.query_single(s, t);
    });

    benchmark_algo("DASH-Bidir", [&](NodeId s, NodeId t) {
        return dash_solver.query_bidir(s, t);
    });

    benchmark_algo("DASH-Auto", [&](NodeId s, NodeId t) {
        return dash_solver.query(s, t);
    });

    benchmark_algo("PHOTON (Auto)", [&](NodeId s, NodeId t) {
        return shortest_path(graph, s, t, Strategy::AUTO);
    });

    return dataset_result;
}

static void print_results(const DatasetResult& dataset_result) {
    std::cout << "  Algorithm          │  Avg Time  │ Speedup │ Opt%   │ Nodes/Query\n";
    std::cout << "  ───────────────────┼────────────┼─────────┼────────┼────────────\n";
    for (const auto& r : dataset_result.results) {
        std::cout << "  " << std::left << std::setw(18) << r.algorithm
                  << " │ " << std::right << std::setw(8) << std::fixed << std::setprecision(1) << r.avg_time_us << " μs"
                  << " │ " << std::setw(6) << std::setprecision(2) << r.speedup << "×"
                  << " │ " << std::setw(5) << std::setprecision(1) << r.optimality_pct << "%"
                  << (r.optimality_pct >= 99.0 ? " ✓" : "  ")
                  << " │ " << std::setw(8) << std::setprecision(0) << r.avg_nodes
                  << "\n";
    }
    std::cout << "\n";
}

static void export_csv(const std::string& dataset_name, const DatasetResult& result, std::ofstream& csv) {
    for (const auto& r : result.results) {
        csv << dataset_name << "," << r.algorithm << "," << r.avg_time_us << "," << r.speedup
            << "," << r.optimality_pct << "," << r.avg_nodes << "\n";
    }
}

// =============================================================================
// MAIN
// =============================================================================

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════════════════════╗
║     ____    _    ____  _   _                                                           ║
║    |  _ \  / \  / ___|| | | |  REAL-WORLD SNAP BENCHMARK                               ║
║    | | | |/ _ \ \___ \| |_| |  Major Algorithms Across SNAP Datasets                    ║
║    | |_| / ___ \ ___) |  _  |                                                          ║
║    |____/_/   \_\____/|_| |_|                                                          ║
║                                                                                        ║
╚════════════════════════════════════════════════════════════════════════════════════════╝
)" << std::endl;

    const std::string data_dir = "/Users/bhaweshbhaskar/Desktop/isitpossible/data/snap/";

    struct Dataset {
        std::string filename;
        std::string name;
        bool undirected;
    };

    std::vector<Dataset> datasets = {
        {"facebook_combined.txt", "Facebook Social Network", true},
        {"ca-GrQc.txt", "Arxiv GR-QC Collaboration", true},
        {"email-Enron.txt", "Email-Enron", true},
        {"soc-Epinions1.txt", "Epinions Trust", true},
        {"wiki-Vote.txt", "Wikipedia Vote", true},
    };

    std::ofstream csv("/Users/bhaweshbhaskar/Desktop/isitpossible/research/real_world_results.csv");
    csv << "dataset,algorithm,avg_time_us,speedup,optimality_pct,avg_nodes\n";

    const size_t NUM_QUERIES = 100;

    for (const auto& ds : datasets) {
        std::string filepath = data_dir + ds.filename;

        std::cout << "\n════════════════════════════════════════════════════════════════════════════════\n";
        std::cout << "  Loading: " << ds.name << "\n";
        std::cout << "════════════════════════════════════════════════════════════════════════════════\n";

        try {
            auto start = std::chrono::high_resolution_clock::now();
            Graph graph = SNAPLoader::load_edge_list(filepath, ds.undirected);
            auto end = std::chrono::high_resolution_clock::now();

            double load_time = std::chrono::duration<double, std::milli>(end - start).count();
            std::cout << "  Load time: " << std::fixed << std::setprecision(0) << load_time << " ms\n";

            SNAPLoader::print_graph_stats(graph, ds.name);

            std::cout << "  Running " << NUM_QUERIES << " random queries...\n\n";
            auto results = run_benchmark(graph, NUM_QUERIES);
            results.dataset_name = ds.name;
            print_results(results);
            export_csv(ds.name, results, csv);

        } catch (const std::exception& e) {
            std::cerr << "  ❌ Error: " << e.what() << "\n";
        }
    }

    std::cout << "\nResults exported to /Users/bhaweshbhaskar/Desktop/isitpossible/research/real_world_results.csv\n";
    return 0;
}
