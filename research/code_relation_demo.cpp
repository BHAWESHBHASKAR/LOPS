/**
 * Code Relation Analysis with DASH
 * 
 * Demonstrates how to use the Graph Engine to inspect code relations.
 * 
 * Scenario:
 * 1. Lexer/Parser extracts symbols (Nodes) and relations (Edges).
 * 2. We build a specialized 'DependencyGraph'.
 * 3. We use DASH to find the "impact path" (e.g., if I change API X, what breaks?).
 */

#include "../include/photon/dash.hpp"
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>
#include <queue>

using namespace photon;
using namespace photon::dash;

// =============================================================================
// 1. SYMBOL TABLE & PARSER SIMULATION
// =============================================================================

// Represents a code symbol (Function, Class, File)
struct Symbol {
    std::string name;
    std::string type; // "function", "class", "file"
};

class CodeParser {
public:
    // Simulates parsing code and finding relations
    // In a real app, this would use Clang LibTooling or similar
    std::pair<Graph, std::map<NodeId, Symbol>> parse_project() {
        std::map<NodeId, Symbol> symbol_table;
        std::map<std::string, NodeId> name_to_id;
        
        auto add_symbol = [&](const std::string& name, const std::string& type) {
            NodeId id = symbol_table.size();
            symbol_table[id] = {name, type};
            name_to_id[name] = id;
            return id;
        };

        // 1. Define Symbols (The "Nodes")
        // Core Logic
        auto main = add_symbol("main()", "function");
        auto app_init = add_symbol("App::init()", "function");
        auto db_connect = add_symbol("Database::connect()", "function");
        auto db_query = add_symbol("Database::query()", "function");
        
        // Utils
        auto log_info = add_symbol("Logger::info()", "function");
        auto str_split = add_symbol("String::split()", "function");
        auto file_read = add_symbol("File::read()", "function");
        
        // Auth System
        auto auth_login = add_symbol("Auth::login()", "function");
        auto auth_verify = add_symbol("Auth::verify_token()", "function");
        auto crypto_hash = add_symbol("Crypto::hash()", "function");
        
        // 2. Define Relations (The "Edges" - Calls/Dependencies)
        GraphBuilder builder(symbol_table.size());
        
        // main calls app logic
        builder.add_edge(main, app_init);
        builder.add_edge(main, log_info);
        
        // App logic calls DB and Auth
        builder.add_edge(app_init, db_connect);
        builder.add_edge(app_init, auth_login);
        builder.add_edge(app_init, log_info);
        
        // Auth calls DB and Crypto
        builder.add_edge(auth_login, auth_verify);
        builder.add_edge(auth_verify, db_query);
        builder.add_edge(auth_verify, crypto_hash);
        
        // DB calls File and Logger
        builder.add_edge(db_connect, file_read); // Read config
        builder.add_edge(db_connect, log_info);
        builder.add_edge(db_query, log_info);
        
        // Utilities
        builder.add_edge(file_read, str_split);
        builder.add_edge(log_info, file_read); // Write to log file
        
        return {builder.build(), symbol_table};
    }
};

// =============================================================================
// MAIN
// =============================================================================

int main() {
    std::cout << R"(
╔════════════════════════════════════════════════════════════════════════════════════════╗
║     ____          _        ____      _       _   _                                 ║
║    / ___|___   __| | ___  |  _ \ ___| | __ _| |_(_) ___  _ __                      ║
║   | |   / _ \ / _` |/ _ \ | |_) / _ \ |/ _` | __| |/ _ \| '_ \                     ║
║   | |__| (_) | (_| |  __/ |  _ <  __/ | (_| | |_| | (_) | | | |                    ║
║    \____\___/ \__,_|\___| |_| \_\___|_|\__,_|\__|_|\___/|_| |_|                    ║
║                                                                                        ║
╚════════════════════════════════════════════════════════════════════════════════════════╝
    Code Relation Analysis Demo using DASH Graph Engine
)" << std::endl;

    CodeParser parser;
    auto [graph, symbols] = parser.parse_project();
    
    std::cout << "  Parsed " << symbols.size() << " symbols and " << graph.num_edges() << " relations.\n\n";

    // Preprocess with DASH
    DASH dash;
    dash.preprocess(graph);
    
    // SCENARIO 1: Impact Analysis
    // "If I change Crypto::hash(), does it affect main()?"
    // This is equivalent to finding a path from main() to Crypto::hash()
    
    NodeId target = 9; // Crypto::hash
    NodeId start = 0;  // main
    
    std::cout << "  🔍 Impact Analysis: Connecting '" << symbols[start].name 
              << "' to '" << symbols[target].name << "'...\n";
              
    auto result = dash.query(start, target);
    
    if (result.found()) {
        std::cout << "  ✓ DEPENDENCY FOUND!\n";
        std::cout << "    Distance: " << result.distance << " hops\n";
        std::cout << "    Call Chain:\n";
        
        // Reconstruct path locally (BFS)
        std::vector<NodeId> path;
        std::queue<std::vector<NodeId>> q;
        q.push({start});
        std::vector<bool> visited(graph.num_nodes(), false);
        visited[start] = true;
        
        while(!q.empty()) {
            auto curr_path = q.front();
            q.pop();
            NodeId u = curr_path.back();
            
            if (u == target) {
                path = curr_path;
                break;
            }
            
            for (const auto& e : graph.out_edges(u)) {
                if (!visited[e.target]) {
                    visited[e.target] = true;
                    auto new_path = curr_path;
                    new_path.push_back(e.target);
                    q.push(new_path);
                }
            }
        }
        
        for (size_t i = 0; i < path.size(); ++i) {
            std::cout << "    " << (i == 0 ? "START " : (i == path.size()-1 ? "END   " : "  ↓   "))
                      << symbols[path[i]].name << "\n";
        }
    } else {
        std::cout << "  ✗ No dependency found.\n";
    }
    
    std::cout << "\n  📊 Structural Insight:\n";
    std::cout << "    Auth::verify_token() Degree: " << graph.out_degree(8) 
              << " (High degree = High coupling)\n";

    return 0;
}
