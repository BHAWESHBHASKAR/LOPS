import React from 'react';
import { motion } from 'framer-motion';

export const MathSection = () => {
    return (
        <div className="grid md:grid-cols-2 gap-16 items-center">
            <div>
                <h2 className="text-4xl font-bold mb-6">The Mathematics of <span className="text-primary">Hubness</span></h2>
                <div className="prose prose-invert text-gray-400">
                    <p className="text-lg mb-6">
                        Traditional algorithms like Dijkstra treat all nodes structurally equal.
                        However, real-world networks (social, web, biological) are
                        <span className="text-white font-medium"> Scale-Free</span> — they possess a heavy-tailed degree distribution.
                    </p>
                    <p className="mb-6">
                        DASH exploits this by introducing a structural "gravity" that pulls the search towards high-degree nodes (hubs),
                        essentially using the network's own topology as a map.
                    </p>

                    <div className="bg-surface p-6 rounded-xl border border-white/5 font-mono text-sm mb-6">
                        <h3 className="text-white mb-4 text-xs uppercase tracking-wider font-bold opacity-70">Core Heuristic Function</h3>
                        <div className="text-center text-lg md:text-xl py-4 text-primary">
                            π(v) = g(v) - α · <span className="text-white">log₂(deg(v)+1)</span> / log₂(max_deg+1)
                        </div>
                        <div className="grid grid-cols-2 gap-4 mt-6 text-xs text-gray-500 border-t border-white/5 pt-4">
                            <div>
                                <span className="text-white block mb-1">g(v)</span>
                                Current distance from source
                            </div>
                            <div>
                                <span className="text-white block mb-1">deg(v)</span>
                                Out-degree of node v
                            </div>
                            <div>
                                <span className="text-white block mb-1">α</span>
                                Adaptive tuning parameter
                            </div>
                            <div>
                                <span className="text-white block mb-1">max_deg</span>
                                Maximum degree in graph
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <div className="relative h-[400px] flex items-center justify-center">
                {/* Abstract Visualization of Hubs */}
                <div className="relative w-full h-full max-w-sm mx-auto">
                    {/* Central Hub */}
                    <motion.div
                        animate={{ scale: [1, 1.1, 1] }}
                        transition={{ duration: 4, repeat: Infinity }}
                        className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-32 h-32 rounded-full bg-primary/20 blur-2xl"
                    />
                    <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-4 h-4 rounded-full bg-white z-10 shadow-[0_0_30px_rgba(255,255,255,0.5)]" />

                    {/* Satellite Nodes */}
                    {[...Array(8)].map((_, i) => (
                        <motion.div
                            key={i}
                            className="absolute top-1/2 left-1/2 w-2 h-2 rounded-full bg-gray-500"
                            style={{
                                rotate: `${i * 45}deg`,
                            }}
                            animate={{
                                x: [0, 80, 0],
                                opacity: [0, 1, 0],
                            }}
                            transition={{
                                duration: 3,
                                delay: i * 0.2,
                                repeat: Infinity,
                                ease: "easeInOut"
                            }}
                        />
                    ))}

                    {/* Orbit Rings */}
                    <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-48 h-48 rounded-full border border-white/10" />
                    <div className="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-72 h-72 rounded-full border border-white/5" />

                    <div className="absolute bottom-0 text-center w-full text-xs text-gray-500">
                        DASH prefers high-degree nodes akin to a gravitational well
                    </div>
                </div>
            </div>
        </div>
    );
};
