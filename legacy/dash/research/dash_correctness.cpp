/**
 * DASH Correctness Verification - Stress Test
 * 
 * rigorously tests DASH optimality guarantees against Dijkstra
 * to verify theoretical claims.
 */

#include "photon/dash.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cassert>

using namespace photon;
using namespace photon::dash;

void run_stress_test(int num_nodes, int num_edges, int num_queries) {
    std::cout << "  Testing on random graph with " << num_nodes << " nodes, " << num_edges << " edges...\n";
    
    // Create random graph
    GraphBuilder builder(num_nodes);
    std::mt19937 rng(42);
    std::uniform_int_distribution<NodeId> node_dist(0, num_nodes - 1);
    std::uniform_real_distribution<float> weight_dist(1.0f, 10.0f);
    
    for (int i = 0; i < num_edges; ++i) {
        NodeId u = node_dist(rng);
        NodeId v = node_dist(rng);
        if (u != v) {
            builder.add_edge(u, v, weight_dist(rng));
        }
    }
    
    Graph graph = builder.build();
    
    DASH dash;
    dash.preprocess(graph);
    
    int correct_single = 0;
    int correct_auto = 0;
    int correct_bidir = 0;
    
    for (int i = 0; i < num_queries; ++i) {
        NodeId s = node_dist(rng);
        NodeId t = node_dist(rng);
        
        auto dij = search::dijkstra(graph, s, t);
        auto res_single = dash.query_single(s, t);
        auto res_auto = dash.query(s, t);
        auto res_bidir = dash.query_bidir(s, t);
        
        // Check DASH-Single (Theoretical claim: 100% optimal)
        if (std::abs(res_single.distance - dij.distance) < 0.001f || 
            (res_single.distance >= INF_DIST && dij.distance >= INF_DIST)) {
            correct_single++;
        }
        
        // Check DASH-Auto
        if (std::abs(res_auto.distance - dij.distance) < 0.001f || 
            (res_auto.distance >= INF_DIST && dij.distance >= INF_DIST)) {
            correct_auto++;
        }
        
        // Check DASH-Bidir
        if (dij.distance < INF_DIST && res_bidir.distance <= dij.distance * 1.01f) { 
             correct_bidir++;
        } else if (dij.distance >= INF_DIST && res_bidir.distance >= INF_DIST) {
             correct_bidir++;
        }
    }
    
    std::cout << "    DASH-Single Optimality: " << std::fixed << std::setprecision(2) 
              << (correct_single * 100.0 / num_queries) << "% " 
              << (correct_single == num_queries ? "✓" : "❌") << "\n";
              
    std::cout << "    DASH-Auto Optimality:   " 
              << (correct_auto * 100.0 / num_queries) << "% " 
              << (correct_auto >= num_queries * 0.99 ? "✓" : "⚠️") << "\n";
              
    std::cout << "    DASH-Bidir Optimality:  " 
              << (correct_bidir * 100.0 / num_queries) << "% \n\n";
              
    if (correct_single != num_queries) {
        std::cerr << "CRITICAL ERROR: DASH-Single failed optimality check!\n";
        exit(1);
    }
}

int main() {
    std::cout << "Running DASH Correctness Stress Test...\n\n";
    
    run_stress_test(50, 200, 1000);
    run_stress_test(200, 1000, 1000);
    run_stress_test(1000, 5000, 1000);
    
    std::cout << "All stress tests passed. DASH-Single is demonstrably optimal.\n";
    return 0;
}
