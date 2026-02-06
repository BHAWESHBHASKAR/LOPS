import React, { useRef, useEffect, useState } from 'react';
import { RefreshCw, Play } from 'lucide-react';

interface Node {
    x: number;
    y: number;
    degree: number;
    id: number;
    isHub: boolean;
}

interface Edge {
    source: number;
    target: number;
}

export const Simulation = () => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [isRunning, setIsRunning] = useState(false);
    const [algorithm, setAlgorithm] = useState<'dijkstra' | 'dash'>('dash');

    // Simulation state
    const nodes = useRef<Node[]>([]);
    const edges = useRef<Edge[]>([]);
    const visited = useRef<Set<number>>(new Set());
    const frontier = useRef<number[]>([]);
    const targetNode = useRef<number>(0);
    const sourceNode = useRef<number>(0);

    const initGraph = () => {
        // Create a scale-free-ish graph
        const n = 150;
        const newNodes: Node[] = [];
        const width = 800;
        const height = 400;

        // Create a few hubs
        const hubs = [
            { x: width * 0.2, y: height * 0.3 },
            { x: width * 0.5, y: height * 0.7 },
            { x: width * 0.8, y: height * 0.4 },
        ];

        // Generate nodes clustered around hubs
        for (let i = 0; i < n; i++) {
            let x, y, isHub = false;

            if (i < hubs.length) {
                x = hubs[i].x;
                y = hubs[i].y;
                isHub = true;
            } else {
                // Pick a random hub to be near
                const hub = hubs[Math.floor(Math.random() * hubs.length)];
                const angle = Math.random() * Math.PI * 2;
                const dist = Math.random() * 150;
                x = hub.x + Math.cos(angle) * dist;
                y = hub.y + Math.sin(angle) * dist;

                // Clamp to canvas
                x = Math.max(20, Math.min(width - 20, x));
                y = Math.max(20, Math.min(height - 20, y));
            }

            newNodes.push({ id: i, x, y, degree: 0, isHub });
        }

        const newEdges: Edge[] = [];
        // Connect nodes based on proximity and preferential attachment
        for (let i = 0; i < n; i++) {
            for (let j = i + 1; j < n; j++) {
                const dist = Math.hypot(newNodes[i].x - newNodes[j].x, newNodes[i].y - newNodes[j].y);
                // More likely to connect if short distance OR if one is a hub
                const prob = (newNodes[i].isHub || newNodes[j].isHub) ? 0.3 : 0.05;

                if (dist < 60 || (dist < 150 && Math.random() < prob)) {
                    newEdges.push({ source: i, target: j });
                    newNodes[i].degree++;
                    newNodes[j].degree++;
                }
            }
        }

        nodes.current = newNodes;
        edges.current = newEdges;
        sourceNode.current = 0; // First hub
        targetNode.current = hubs.length - 1; // Last hub

        resetSearch();
    };

    const resetSearch = () => {
        visited.current = new Set();
        frontier.current = [sourceNode.current];
        setIsRunning(false);
        draw()
    };

    const step = () => {
        if (frontier.current.length === 0) {
            setIsRunning(false);
            return;
        }

        // Naive priority queue simulation
        // Dijkstra: Pick closest to source (simulated by just expanding uniform-ish)
        // DASH: Pick highest degree or closest to hubs

        // Sort frontier based on algo
        frontier.current.sort((a, b) => {
            const nodeA = nodes.current[a];
            const nodeB = nodes.current[b];

            if (algorithm === 'dash') {
                // DASH prefers high degree
                return nodeB.degree - nodeA.degree;
            } else {
                // Dijkstra prefers closer (BFS approx for visual)
                // Random shuffle to simulate uniform expansion without weights
                return 0.5 - Math.random();
            }
        });

        const current = frontier.current.shift()!;
        visited.current.add(current);

        if (current === targetNode.current) {
            setIsRunning(false);
            draw();
            return;
        }

        // Add neighbors
        edges.current.forEach(e => {
            let next = -1;
            if (e.source === current) next = e.target;
            if (e.target === current) next = e.source;

            if (next !== -1 && !visited.current.has(next) && !frontier.current.includes(next)) {
                frontier.current.push(next);
            }
        });

        draw();

        if (isRunning) {
            requestAnimationFrame(step);
        }
    };

    const draw = () => {
        const ctx = canvasRef.current?.getContext('2d');
        if (!ctx) return;

        ctx.clearRect(0, 0, 800, 400);

        // Draw edges
        ctx.strokeStyle = '#27272a';
        ctx.lineWidth = 1;
        edges.current.forEach(e => {
            const s = nodes.current[e.source];
            const t = nodes.current[e.target];
            ctx.beginPath();
            ctx.moveTo(s.x, s.y);
            ctx.lineTo(t.x, t.y);
            ctx.stroke();
        });

        // Draw nodes
        nodes.current.forEach(n => {
            ctx.beginPath();
            ctx.arc(n.x, n.y, n.isHub ? 8 : 3, 0, Math.PI * 2);

            if (n.id === sourceNode.current) {
                ctx.fillStyle = '#22c55e'; // Green source
            } else if (n.id === targetNode.current) {
                ctx.fillStyle = '#ef4444'; // Red target
            } else if (visited.current.has(n.id)) {
                ctx.fillStyle = '#3b82f6'; // Visited Blue
            } else if (frontier.current.includes(n.id)) {
                ctx.fillStyle = '#eab308'; // Frontier Yellow
            } else {
                ctx.fillStyle = n.isHub ? '#52525b' : '#3f3f46';
            }
            ctx.fill();
        });
    };

    useEffect(() => {
        initGraph();
    }, []);

    useEffect(() => {
        let interval: any;
        if (isRunning) {
            interval = setInterval(step, 50);
        }
        return () => clearInterval(interval);
    }, [isRunning, algorithm]);

    return (
        <div className="grid md:grid-cols-3 gap-8 items-start">
            <div className="md:col-span-1 space-y-6">
                <h2 className="text-4xl font-bold mb-4">Live Simulation</h2>
                <p className="text-gray-400">
                    Visualize how DASH prioritizes "hubs" (large nodes) to quickly traverse the graph,
                    whereas Dijkstra expands uniformly (blindly).
                </p>

                <div className="flex gap-4 p-1 bg-surface rounded-lg border border-white/10 w-fit">
                    <button
                        onClick={() => { setAlgorithm('dijkstra'); resetSearch(); }}
                        className={`px-4 py-2 rounded-md text-sm font-medium transition-colors ${algorithm === 'dijkstra' ? 'bg-white text-black' : 'text-gray-400 hover:text-white'}`}
                    >
                        Dijkstra
                    </button>
                    <button
                        onClick={() => { setAlgorithm('dash'); resetSearch(); }}
                        className={`px-4 py-2 rounded-md text-sm font-medium transition-colors ${algorithm === 'dash' ? 'bg-primary text-white' : 'text-gray-400 hover:text-white'}`}
                    >
                        DASH
                    </button>
                </div>

                <div className="flex gap-4">
                    <button
                        onClick={() => setIsRunning(!isRunning)}
                        className="flex items-center gap-2 px-6 py-3 bg-white text-black font-bold rounded-lg hover:bg-gray-200"
                    >
                        <Play size={18} fill={isRunning ? "black" : "none"} />
                        {isRunning ? 'Pause' : 'Start'}
                    </button>
                    <button
                        onClick={initGraph}
                        className="flex items-center gap-2 px-6 py-3 bg-surface border border-white/10 text-white font-bold rounded-lg hover:bg-white/5"
                    >
                        <RefreshCw size={18} />
                        Reset Graph
                    </button>
                </div>
            </div>

            <div className="md:col-span-2 bg-black rounded-xl border border-white/10 overflow-hidden shadow-2xl relative">
                <div className="absolute top-4 left-4 text-xs font-mono text-gray-500 pointer-events-none">
                    {algorithm === 'dash' ? '>> DASH_HEURISTIC_ACTIVE' : '>> UNIFORM_COST_SEARCH'}
                </div>
                <canvas
                    ref={canvasRef}
                    width={800}
                    height={400}
                    className="w-full h-auto block"
                />
            </div>
        </div>
    );
};
