import React from 'react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer } from 'recharts';

const data = [
    { name: 'Dijkstra', speedup: 1.0, color: '#3f3f46' },
    { name: 'ALT (8L)', speedup: 5.3, color: '#a1a1aa' },
    { name: 'DASH (Auto)', speedup: 6.6, color: '#3b82f6' },
    { name: 'DASH (Bidir)', speedup: 16.6, color: '#8b5cf6' },
];

export const Benchmarks = () => {
    return (
        <div>
            <div className="mb-12 text-center">
                <h2 className="text-4xl font-bold mb-4">Performance Benchmarks</h2>
                <p className="text-gray-400 max-w-2xl mx-auto">
                    Comparison on a 5,000-node Scale-Free Network (CV = 2.18).
                    <br />
                    DASH outperforms classic algorithms significantly without expensive preprocessing.
                </p>
            </div>

            <div className="h-[400px] w-full bg-surface p-8 rounded-2xl border border-white/5">
                <ResponsiveContainer width="100%" height="100%">
                    <BarChart
                        data={data}
                        layout="vertical"
                        margin={{ top: 5, right: 30, left: 40, bottom: 5 }}
                        barSize={40}
                    >
                        <CartesianGrid strokeDasharray="3 3" horizontal={false} stroke="#27272a" />
                        <XAxis type="number" stroke="#71717a" />
                        <YAxis type="category" dataKey="name" stroke="#a1a1aa" width={100} />
                        <Tooltip
                            contentStyle={{ backgroundColor: '#18181b', borderColor: '#27272a', color: '#fff' }}
                            cursor={{ fill: '#27272a' }}
                        />
                        <Bar dataKey="speedup" fill="#8884d8" radius={[0, 4, 4, 0]}>
                            {data.map((entry, index) => (
                                <cell key={`cell-${index}`} fill={entry.color} />
                            ))}
                        </Bar>
                    </BarChart>
                </ResponsiveContainer>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mt-12">
                <div className="p-6 bg-surface rounded-xl border border-white/5">
                    <div className="text-primary font-bold mb-2">Social Networks</div>
                    <div className="text-3xl font-bold text-white mb-1">16×</div>
                    <div className="text-sm text-gray-500">Speedup on high-CV graphs</div>
                </div>
                <div className="p-6 bg-surface rounded-xl border border-white/5">
                    <div className="text-success font-bold mb-2">Optimality</div>
                    <div className="text-3xl font-bold text-white mb-1">100%</div>
                    <div className="text-sm text-gray-500">With DASH-Single variant</div>
                </div>
                <div className="p-6 bg-surface rounded-xl border border-white/5">
                    <div className="text-accent font-bold mb-2">Preprocessing</div>
                    <div className="text-3xl font-bold text-white mb-1">~0 <span className="text-lg">μs</span></div>
                    <div className="text-sm text-gray-500">O(V) vs O(V²) for others</div>
                </div>
            </div>
        </div>
    );
};
