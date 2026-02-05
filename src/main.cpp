/**
 * PHOTON - Main Entry Point
 * 
 * Example usage and quick benchmark.
 */

#include "photon/photon.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace photon;

void print_result(const PathResult& result, const char* name) {
    std::cout << std::left << std::setw(25) << name << " | ";
    
    if (result.found()) {
        std::cout << "dist=" << std::fixed << std::setprecision(2) 
                  << std::setw(8) << result.distance
                  << " | nodes=" << std::setw(8) << result.nodes_explored
                  << " | time=" << std::setw(10) << std::setprecision(1) 
                  << result.time_microseconds << " μs"
                  << " | path_len=" << result.path.size();
    } else {
        std::cout << "NO PATH FOUND";
    }
    std::cout << "\n";
}

void run_benchmark(const Graph& graph, const char* graph_name, NodeId source, NodeId target) {
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "Graph: " << graph_name << " (" << graph.num_nodes() << " nodes, " 
              << graph.num_edges() << " edges)\n";
    std::cout << "Query: " << source << " → " << target << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    // Run each algorithm
    print_result(search::dijkstra(graph, source, target), "Dijkstra");
    print_result(search::bidirectional_simd(graph, source, target), "Bidirectional SIMD");
    print_result(search::a_star(graph, source, target), "A*");
    print_result(search::delta_stepping(graph, source, target), "Delta-Stepping");
    print_result(search::parallel_wavefront(graph, source, target), "Parallel Wavefront");
    
    // PHOTON Auto
    PhotonEngine engine;
    Graph g_copy = std::move(const_cast<Graph&>(graph));  // Temporary for demo
    engine.load_graph(std::move(g_copy));
    print_result(engine.find_path(source, target), "PHOTON (Auto)");
}

int main(int argc, char** argv) {
    // Print banner
    std::cout << R"(
    ██████╗ ██╗  ██╗ ██████╗ ████████╗ ██████╗ ███╗   ██╗
    ██╔══██╗██║  ██║██╔═══██╗╚══██╔══╝██╔═══██╗████╗  ██║
    ██████╔╝███████║██║   ██║   ██║   ██║   ██║██╔██╗ ██║
    ██╔═══╝ ██╔══██║██║   ██║   ██║   ██║   ██║██║╚██╗██║
    ██║     ██║  ██║╚██████╔╝   ██║   ╚██████╔╝██║ ╚████║
    ╚═╝     ╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═══╝
                    
         Speed of Light Pathfinding v1.0.0
)" << std::endl;

    std::cout << "SIMD Width: " << SIMD_WIDTH << " floats\n";
    std::cout << "Threads: " << std::thread::hardware_concurrency() << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    // =========================================================================
    // Benchmark 1: Grid Graph
    // =========================================================================
    {
        auto grid = generators::grid(100, 100, true, true);
        run_benchmark(grid, "Grid 100x100 (8-connected)", 0, 99 * 100 + 99);
    }
    
    // =========================================================================
    // Benchmark 2: Social Network
    // =========================================================================
    {
        auto social = generators::social_network(5000, 10);
        run_benchmark(social, "Social Network (5000 nodes)", 0, 4999);
    }
    
    // =========================================================================
    // Benchmark 3: Road Network
    // =========================================================================
    {
        auto road = generators::road_network(2000, 6);
        run_benchmark(road, "Road Network (2000 nodes)", 0, 1999);
    }
    
    // =========================================================================
    // Benchmark 4: Perfect Maze
    // =========================================================================
    {
        auto maze_graph = generators::maze(50, 50);
        run_benchmark(maze_graph, "Perfect Maze 50x50", 0, 50 * 50 - 1);
    }
    
    // =========================================================================
    // Benchmark 5: Scale-Free Network
    // =========================================================================
    {
        auto scale = generators::scale_free(3000, 2.5f);
        run_benchmark(scale, "Scale-Free Network (3000 nodes)", 0, 2999);
    }
    
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "Benchmarks complete!\n";
    
    return 0;
}
