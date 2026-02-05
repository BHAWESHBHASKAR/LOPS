cmake_minimum_required(VERSION 3.20)
project(photon VERSION 1.0.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(PHOTON_ARM64 TRUE)
    message(STATUS "Detected ARM64 architecture - using NEON SIMD")
else()
    set(PHOTON_X86 TRUE)
    message(STATUS "Detected x86 architecture - using AVX/SSE SIMD")
endif()

# Release optimizations
if(PHOTON_ARM64)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -mcpu=native -flto -ffast-math -DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -ffast-math -DNDEBUG")
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Find threads
find_package(Threads REQUIRED)

# Optional: Find OpenMP for additional parallelism
find_package(OpenMP QUIET)

# Optional: Find Intel TBB for advanced parallel algorithms
find_package(TBB QUIET)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Core library
add_library(photon_core STATIC
    src/graph/graph.cpp
    src/graph/graph_builder.cpp
    src/graph/morton_layout.cpp
    src/queue/simd_queue.cpp
    src/search/dijkstra.cpp
    src/search/bidirectional.cpp
    src/search/delta_stepping.cpp
    src/search/micro_ch.cpp
    src/search/parallel_wave.cpp
    src/engine/photon.cpp
    src/engine/strategy_selector.cpp
    src/cache/query_cache.cpp
    src/landmarks/learned_landmarks.cpp
)

target_link_libraries(photon_core
    Threads::Threads
    $<$<BOOL:${OpenMP_FOUND}>:OpenMP::OpenMP_CXX>
    $<$<BOOL:${TBB_FOUND}>:TBB::tbb>
)

# Add SIMD flags based on architecture
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    if(PHOTON_X86)
        # x86 SIMD flags
        target_compile_options(photon_core PRIVATE
            -mavx2
            -mfma
        )
    endif()
    # ARM64 uses NEON which is enabled by default
endif()

# Define architecture macro
if(PHOTON_ARM64)
    target_compile_definitions(photon_core PUBLIC PHOTON_ARCH_ARM64=1)
else()
    target_compile_definitions(photon_core PUBLIC PHOTON_ARCH_X86=1)
endif()

# Main executable
add_executable(photon src/main.cpp)
target_link_libraries(photon photon_core)

# Benchmark executable
add_executable(photon_bench benchmarks/bench_main.cpp)
target_link_libraries(photon_bench photon_core)

# Tests
enable_testing()
add_executable(photon_tests
    tests/test_main.cpp
    tests/test_graph.cpp
    tests/test_simd_queue.cpp
    tests/test_search.cpp
)
target_link_libraries(photon_tests photon_core)
add_test(NAME photon_tests COMMAND photon_tests)

# Research experiments (novel algorithms)
add_executable(photon_research research/experiment.cpp)
target_link_libraries(photon_research photon_core)
target_include_directories(photon_research PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Entropy deep dive research
add_executable(entropy_research research/entropy_deep_dive.cpp)
target_link_libraries(entropy_research photon_core)
target_include_directories(entropy_research PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Deep formula exploration
add_executable(formula_exploration research/deep_formula_exploration.cpp)
target_link_libraries(formula_exploration photon_core)
target_include_directories(formula_exploration PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Install
install(TARGETS photon photon_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)

# Bidirectional exploration
add_executable(bidir_exploration research/bidir_exploration.cpp)
target_link_libraries(bidir_exploration photon_core)
target_include_directories(bidir_exploration PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Advanced algorithm exploration
add_executable(advanced_exploration research/advanced_exploration.cpp)
target_link_libraries(advanced_exploration photon_core)
target_include_directories(advanced_exploration PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Final algorithm
add_executable(final_algorithm research/final_algorithm.cpp)
target_link_libraries(final_algorithm photon_core)
target_include_directories(final_algorithm PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Ultra optimized
add_executable(ultra_optimized research/ultra_optimized.cpp)
target_link_libraries(ultra_optimized photon_core)
target_include_directories(ultra_optimized PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Algorithm comparison
add_executable(algorithm_comparison research/algorithm_comparison.cpp)
target_link_libraries(algorithm_comparison photon_core)
target_include_directories(algorithm_comparison PRIVATE ${CMAKE_SOURCE_DIR}/research)

# Ultra optimized EWD
add_executable(ewd_ultra research/ewd_ultra_optimized.cpp)
target_link_libraries(ewd_ultra photon_core)
target_include_directories(ewd_ultra PRIVATE ${CMAKE_SOURCE_DIR}/research)

# EWD Ultimate
add_executable(ewd_ultimate research/ewd_ultimate.cpp)
target_link_libraries(ewd_ultimate photon_core)
target_include_directories(ewd_ultimate PRIVATE ${CMAKE_SOURCE_DIR}/research)

# EWD Perfect
add_executable(ewd_perfect research/ewd_perfect.cpp)
target_link_libraries(ewd_perfect photon_core)
target_include_directories(ewd_perfect PRIVATE ${CMAKE_SOURCE_DIR}/research)

# EWD Final
add_executable(ewd_final research/ewd_final.cpp)
target_link_libraries(ewd_final photon_core)
target_include_directories(ewd_final PRIVATE ${CMAKE_SOURCE_DIR}/research)

# DASH Benchmark
add_executable(dash_benchmark research/dash_benchmark.cpp)
target_link_libraries(dash_benchmark photon_core)
target_include_directories(dash_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/research)

# DASH Research
add_executable(dash_research research/dash_research.cpp)
target_link_libraries(dash_research photon_core)
target_include_directories(dash_research PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/research)

# DASH Publication Analysis
add_executable(dash_publication research/dash_publication.cpp)
target_link_libraries(dash_publication photon_core)
target_include_directories(dash_publication PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/research)
