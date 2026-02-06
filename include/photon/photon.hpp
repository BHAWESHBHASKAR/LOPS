/**
 * PHOTON - Speed of Light Pathfinding
 * 
 * Single include header for the PHOTON library.
 */

#pragma once

#include "photon/core.hpp"
#include "photon/graph.hpp"
#include "photon/simd_queue.hpp"
#include "photon/search.hpp"
#include "photon/lops.hpp"
#include "photon/engine.hpp"

namespace photon {

// Version info
constexpr int VERSION_MAJOR = 1;
constexpr int VERSION_MINOR = 0;
constexpr int VERSION_PATCH = 0;
constexpr const char* VERSION_STRING = "1.0.0";

// Print banner
inline void print_banner() {
    std::printf(R"(
    ██████╗ ██╗  ██╗ ██████╗ ████████╗ ██████╗ ███╗   ██╗
    ██╔══██╗██║  ██║██╔═══██╗╚══██╔══╝██╔═══██╗████╗  ██║
    ██████╔╝███████║██║   ██║   ██║   ██║   ██║██╔██╗ ██║
    ██╔═══╝ ██╔══██║██║   ██║   ██║   ██║   ██║██║╚██╗██║
    ██║     ██║  ██║╚██████╔╝   ██║   ╚██████╔╝██║ ╚████║
    ╚═╝     ╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝ ╚═╝  ╚═══╝
)");
    std::printf("         Speed of Light Pathfinding v%s\n", VERSION_STRING);
    std::printf("         SIMD Width: %zu | Threads: %u\n\n", 
        SIMD_WIDTH, std::thread::hardware_concurrency());
}

} // namespace photon
