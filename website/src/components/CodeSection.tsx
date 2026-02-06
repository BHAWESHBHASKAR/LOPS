import React from 'react';
import { Lightbulb } from 'lucide-react';

const cppCode = `// DASH: Degree-Adaptive Shortest-path Heuristic
// Core implementation of the priority function

float DASH::heuristic(NodeId u, NodeId t) const {
    // Standard heuristic (e.g., Euclidean or zero)
    float h_val = 0; 

    // Degree-based potential field
    // We logarithmically dampen the degree to prevent over-aggression
    float degree_bonus = std::log2(graph_->out_degree(u) + 1.0f) / 
                         std::log2(max_degree_ + 1.0f);

    // Alpha tuning parameter (adapted based on Graph CV)
    // Higher CV -> Stronger Hub structure -> Higher Alpha
    float alpha_term = alpha_ * degree_bonus;

    // The "pull" towards hubs acts as a negative cost
    // making these nodes appear "closer" to the search frontier
    return h_val - alpha_term;
}

// Integration into search
void DASH::query(NodeId s, NodeId t) {
    // ... priority queue setup ... 
    
    while (!pq.empty()) {
        auto [p, u] = pq.top(); pq.pop();
        
        // Classic relaxation
        for (auto& edge : graph_->edges(u)) {
            float new_dist = dist[u] + edge.weight;
            
            if (new_dist < dist[edge.target]) {
                dist[edge.target] = new_dist;
                
                // Key difference: Priority includes degree bonus
                float priority = new_dist + heuristic(edge.target, t);
                pq.push({priority, edge.target});
            }
        }
    }
}`;

export const CodeSection = () => {
    return (
        <div className="grid md:grid-cols-2 gap-12 items-start">
            <div className="order-2 md:order-1 font-mono text-sm bg-black rounded-xl p-6 border border-white/10 overflow-x-auto shadow-2xl">
                <pre className="text-gray-300">
                    <code dangerouslySetInnerHTML={{
                        __html: cppCode
                            .replace(/\/\/.*$/gm, '<span class="text-gray-500">$&</span>')
                            .replace(/\b(float|void|return|if|while|for|auto|const)\b/g, '<span class="text-accent">$&</span>')
                            .replace(/\b(DASH|NodeId|std::log2|graph_|out_degree|max_degree_)\b/g, '<span class="text-primary">$&</span>')
                    }} />
                </pre>
            </div>

            <div className="order-1 md:order-2">
                <h2 className="text-4xl font-bold mb-6">Drop-in Implementation</h2>
                <p className="text-gray-400 text-lg mb-8">
                    DASH requires minimal changes to existing Dijkstra/A* implementations.
                    The core logic resides entirely within the priority function.
                </p>

                <div className="space-y-6">
                    <div className="flex gap-4">
                        <div className="mt-1 bg-primary/10 p-2 rounded-lg text-primary">
                            <Lightbulb size={20} />
                        </div>
                        <div>
                            <h3 className="font-bold text-white mb-2">Zero Preprocessing</h3>
                            <p className="text-sm text-gray-500">
                                Unlike ALT or Contraction Hierarchies, DASH calculates its potential field on-the-fly
                                using O(1) degree lookups. Ideally suited for large, dynamic graphs.
                            </p>
                        </div>
                    </div>

                    <div className="flex gap-4">
                        <div className="mt-1 bg-accent/10 p-2 rounded-lg text-accent">
                            <Lightbulb size={20} />
                        </div>
                        <div>
                            <h3 className="font-bold text-white mb-2">Memory Efficient</h3>
                            <p className="text-sm text-gray-500">
                                No auxiliary structures needed. Uses the graph topology itself as the index.
                                Memory overhead is effectively zero bytes.
                            </p>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};
