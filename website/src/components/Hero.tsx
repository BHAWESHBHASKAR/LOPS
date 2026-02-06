import React from 'react';
import { motion } from 'framer-motion';
import { ArrowRight, Download, FileText } from 'lucide-react';

export const Hero = ({ setActiveSection }: { setActiveSection: (s: string) => void }) => {
    return (
        <div className="relative min-h-[90vh] flex items-center justify-center overflow-hidden">
            {/* Background Gradient */}
            <div className="absolute inset-0 bg-gradient-to-b from-primary/20 via-background to-background pointer-events-none" />

            {/* Animated Mesh Background (CSS only for perf) */}
            <div className="absolute inset-0 opacity-20"
                style={{
                    backgroundImage: 'radial-gradient(circle at center, #14b8a6 1px, transparent 1px)',
                    backgroundSize: '40px 40px'
                }}
            />

            <div className="relative z-10 max-w-4xl mx-auto px-6 text-center">
                <motion.div
                    initial={{ opacity: 0, y: 20 }}
                    animate={{ opacity: 1, y: 0 }}
                    transition={{ duration: 0.8 }}
                >
                    <div className="inline-block mb-4 px-4 py-1.5 rounded-full border border-primary/20 bg-primary/10 text-primary text-sm font-medium">
                        New Research 2026
                    </div>

                    <h1 className="text-6xl md:text-8xl font-bold tracking-tighter mb-6 bg-clip-text text-transparent bg-gradient-to-r from-white via-gray-200 to-gray-500">
                        Pathfinding Lab
                    </h1>

                    <p className="text-xl md:text-2xl text-gray-400 mb-8 max-w-2xl mx-auto leading-relaxed">
                        Multi-algorithm pathfinding lab.
                        <br />
                        <span className="text-white font-medium">24× faster</span> than Dijkstra.
                        <span className="text-white font-medium"> Exact + Approx</span>.
                        <br />
                        Real-world graph focus.
                    </p>

                    <div className="flex flex-col sm:flex-row items-center justify-center gap-4">
                        <button
                            onClick={() => {
                                setActiveSection('visualizers');
                                document.getElementById('visualizers')?.scrollIntoView({ behavior: 'smooth' });
                            }}
                            className="px-8 py-4 bg-white text-black font-bold rounded-lg hover:bg-gray-200 transition-colors flex items-center gap-2 w-full sm:w-auto justify-center"
                        >
                            Visualizers <ArrowRight size={18} />
                        </button>
                        <button
                            onClick={() => {
                                setActiveSection('math');
                                document.getElementById('math')?.scrollIntoView({ behavior: 'smooth' });
                            }}
                            className="px-8 py-4 bg-surface border border-white/10 text-white font-medium rounded-lg hover:bg-white/5 transition-colors flex items-center gap-2 w-full sm:w-auto justify-center"
                        >
                            How it Works
                        </button>
                    </div>
                </motion.div>

                {/* Stats Grid */}
                <motion.div
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    transition={{ delay: 0.5, duration: 0.8 }}
                    className="grid grid-cols-2 md:grid-cols-4 gap-8 mt-20 border-t border-white/5 pt-12"
                >
                    {[
                        { label: 'Speedup', value: '16x', sub: 'vs Dijkstra' },
                        { label: 'Optimality', value: '100%', sub: 'DASH-Single' },
                        { label: 'Preprocessing', value: '0ms', sub: 'O(V) Complexity' },
                        { label: 'Target', value: 'Social', sub: 'Scale-Free Graphs' },
                    ].map((stat, i) => (
                        <div key={i} className="text-center">
                            <div className="text-3xl font-bold text-white mb-1">{stat.value}</div>
                            <div className="text-sm text-gray-400 font-medium">{stat.label}</div>
                            <div className="text-xs text-gray-600 mt-1">{stat.sub}</div>
                        </div>
                    ))}
                </motion.div>
            </div>
        </div>
    );
};
