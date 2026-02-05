/**
 * PHOTON Test Suite
 */

#include "photon/photon.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace photon;

#define TEST(name) void test_##name(); \
    struct test_##name##_register { \
        test_##name##_register() { \
            std::cout << "Running " #name "... "; \
            test_##name(); \
            std::cout << "✓\n"; \
        } \
    } test_##name##_instance; \
    void test_##name()

#define ASSERT(cond) do { if (!(cond)) { \
    std::cerr << "\nAssertion failed: " #cond "\n  at " __FILE__ ":" << __LINE__ << "\n"; \
    std::exit(1); \
}} while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
    std::cerr << "\nAssertion failed: " #a " == " #b "\n  " << (a) << " != " << (b) << "\n  at " __FILE__ ":" << __LINE__ << "\n"; \
    std::exit(1); \
}} while(0)

#define ASSERT_NEAR(a, b, eps) do { if (std::abs((a) - (b)) > (eps)) { \
    std::cerr << "\nAssertion failed: |" #a " - " #b "| <= " #eps "\n  |" << (a) << " - " << (b) << "| = " << std::abs((a)-(b)) << "\n  at " __FILE__ ":" << __LINE__ << "\n"; \
    std::exit(1); \
}} while(0)

// =============================================================================
// Graph Tests
// =============================================================================

TEST(graph_builder_basic) {
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f)
           .add_edge(1, 2, 2.0f)
           .add_edge(0, 2, 5.0f);
    
    auto graph = builder.build();
    
    ASSERT_EQ(graph.num_nodes(), 3);
    ASSERT_EQ(graph.num_edges(), 3);
    ASSERT_EQ(graph.out_degree(0), 2);
    ASSERT_EQ(graph.out_degree(1), 1);
    ASSERT_EQ(graph.out_degree(2), 0);
}

TEST(graph_builder_undirected) {
    GraphBuilder builder;
    builder.add_undirected_edge(0, 1, 1.0f)
           .add_undirected_edge(1, 2, 2.0f);
    
    auto graph = builder.build();
    
    ASSERT_EQ(graph.num_edges(), 4);  // 2 undirected = 4 directed
    ASSERT_EQ(graph.out_degree(0), 1);
    ASSERT_EQ(graph.out_degree(1), 2);
    ASSERT_EQ(graph.out_degree(2), 1);
}

TEST(graph_reverse) {
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f)
           .add_edge(0, 2, 2.0f)
           .add_edge(1, 2, 3.0f)
           .build_reverse(true);
    
    auto graph = builder.build();
    
    ASSERT(graph.has_reverse());
    ASSERT_EQ(graph.in_degree(0), 0);
    ASSERT_EQ(graph.in_degree(1), 1);
    ASSERT_EQ(graph.in_degree(2), 2);
}

TEST(graph_coordinates) {
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f)
           .set_coordinate(0, 0.0f, 0.0f)
           .set_coordinate(1, 3.0f, 4.0f);
    
    auto graph = builder.build();
    
    ASSERT(graph.has_coordinates());
    ASSERT_NEAR(graph.euclidean_distance(0, 1), 5.0f, 0.001f);
}

// =============================================================================
// Search Tests
// =============================================================================

TEST(dijkstra_simple) {
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f)
           .add_edge(1, 2, 2.0f)
           .add_edge(0, 2, 5.0f);
    
    auto graph = builder.build();
    auto result = search::dijkstra(graph, 0, 2);
    
    ASSERT(result.found());
    ASSERT_NEAR(result.distance, 3.0f, 0.001f);
    ASSERT_EQ(result.path.size(), 3);
    ASSERT_EQ(result.path[0], 0);
    ASSERT_EQ(result.path[1], 1);
    ASSERT_EQ(result.path[2], 2);
}

TEST(dijkstra_no_path) {
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f)
           .add_edge(2, 3, 1.0f);  // Disconnected
    
    auto graph = builder.build();
    auto result = search::dijkstra(graph, 0, 3);
    
    ASSERT(!result.found());
}

TEST(dijkstra_same_node) {
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f);
    
    auto graph = builder.build();
    auto result = search::dijkstra(graph, 0, 0);
    
    ASSERT(result.found());
    ASSERT_NEAR(result.distance, 0.0f, 0.001f);
    ASSERT_EQ(result.path.size(), 1);
}

TEST(bidirectional_correctness) {
    // Create a graph where bidirectional should find same answer as Dijkstra
    GraphBuilder builder;
    for (int i = 0; i < 10; ++i) {
        builder.add_undirected_edge(i, i + 1, 1.0f);
        if (i + 2 < 11) {
            builder.add_undirected_edge(i, i + 2, 2.5f);
        }
    }
    
    auto graph = builder.build();
    
    auto dij_result = search::dijkstra(graph, 0, 10);
    auto bid_result = search::bidirectional_simd(graph, 0, 10);
    
    ASSERT(dij_result.found());
    ASSERT(bid_result.found());
    ASSERT_NEAR(dij_result.distance, bid_result.distance, 0.001f);
}

TEST(astar_with_heuristic) {
    // Grid with coordinates
    auto grid = generators::grid(10, 10, false, false);
    
    auto result = search::a_star(grid, 0, 99);
    
    ASSERT(result.found());
    ASSERT_NEAR(result.distance, 18.0f, 0.001f);  // Manhattan distance on 10x10 grid
}

// =============================================================================
// Generator Tests
// =============================================================================

TEST(generator_grid) {
    auto grid = generators::grid(10, 10, false, false);
    
    ASSERT_EQ(grid.num_nodes(), 100);
    ASSERT(grid.has_coordinates());
    
    // Corner nodes should have degree 2
    ASSERT_EQ(grid.out_degree(0), 2);
    ASSERT_EQ(grid.out_degree(9), 2);
    ASSERT_EQ(grid.out_degree(90), 2);
    ASSERT_EQ(grid.out_degree(99), 2);
    
    // Center nodes should have degree 4
    ASSERT_EQ(grid.out_degree(55), 4);
}

TEST(generator_social_network) {
    auto social = generators::social_network(100, 5);
    
    ASSERT_EQ(social.num_nodes(), 100);
    ASSERT(social.stats().clustering_coefficient > 0);
}

TEST(generator_maze) {
    auto maze = generators::maze(10, 10);
    
    ASSERT_EQ(maze.num_nodes(), 100);
    
    // Perfect maze: exactly n-1 undirected edges = 2*(n-1) directed
    ASSERT_EQ(maze.num_edges(), 2 * 99);
    
    // Should be connected - path from corner to corner
    auto result = search::dijkstra(maze, 0, 99);
    ASSERT(result.found());
}

// =============================================================================
// Engine Tests
// =============================================================================

TEST(engine_basic) {
    PhotonEngine engine;
    
    GraphBuilder builder;
    builder.add_edge(0, 1, 1.0f)
           .add_edge(1, 2, 1.0f)
           .add_edge(0, 2, 3.0f);
    
    engine.load_graph(builder.build());
    
    ASSERT(engine.ready());
    
    auto result = engine.find_path(0, 2);
    ASSERT(result.found());
    ASSERT_NEAR(result.distance, 2.0f, 0.001f);
}

TEST(engine_cache) {
    PhotonConfig config;
    config.enable_query_cache = true;
    PhotonEngine engine(config);
    
    engine.load_graph(generators::grid(20, 20, true, true));
    
    // First query
    auto result1 = engine.find_path(0, 399);
    ASSERT(result1.found());
    
    // Second query (should be cached)
    auto result2 = engine.find_path(0, 399);
    ASSERT(result2.found());
    ASSERT_NEAR(result1.distance, result2.distance, 0.001f);
}

// =============================================================================
// SIMD Queue Tests
// =============================================================================

TEST(simd_queue_basic) {
    SIMDPriorityQueue pq(100);
    
    pq.push(5, 1.0f);
    pq.push(3, 0.5f);
    pq.push(7, 2.0f);
    
    auto [d1, n1] = pq.pop();
    ASSERT_EQ(n1, 3);
    ASSERT_NEAR(d1, 0.5f, 0.001f);
    
    auto [d2, n2] = pq.pop();
    ASSERT_EQ(n2, 5);
    ASSERT_NEAR(d2, 1.0f, 0.001f);
    
    auto [d3, n3] = pq.pop();
    ASSERT_EQ(n3, 7);
    ASSERT_NEAR(d3, 2.0f, 0.001f);
}

TEST(simd_queue_decrease_key) {
    SIMDPriorityQueue pq(100);
    
    pq.push(5, 10.0f);
    pq.push(5, 5.0f);   // Decrease key
    pq.push(5, 3.0f);   // Decrease again
    
    auto [d, n] = pq.pop();
    ASSERT_EQ(n, 5);
    ASSERT_NEAR(d, 3.0f, 0.001f);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              PHOTON TEST SUITE                               ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";
    
    // Tests are auto-registered and run via static initialization
    
    std::cout << "\n══════════════════════════════════════════════════════════════════\n";
    std::cout << "All tests passed! ✓\n";
    std::cout << "══════════════════════════════════════════════════════════════════\n\n";
    
    return 0;
}
