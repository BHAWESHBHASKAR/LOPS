import React, { useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { Rocket, Zap, BookOpen, Activity, GitBranch, Github, ChevronRight } from 'lucide-react';
import { Hero } from './components/Hero';
import { MathSection } from './components/MathSection';
import { Benchmarks } from './components/Benchmarks';
import { Simulation } from './components/Simulation';
import { CodeSection } from './components/CodeSection';

function App() {
  const [activeSection, setActiveSection] = useState('hero');

  const navItems = [
    { id: 'hero', label: 'Overview', icon: Rocket },
    { id: 'math', label: 'The Math', icon: BookOpen },
    { id: 'benchmarks', label: 'Benchmarks', icon: Activity },
    { id: 'simulation', label: 'Live Demo', icon: Zap },
    { id: 'code', label: 'Implementation', icon: GitBranch },
  ];

  return (
    <div className="min-h-screen bg-background text-white selection:bg-primary/30">
      {/* Navigation */}
      <nav className="fixed top-0 w-full z-50 bg-surface/80 backdrop-blur-md border-b border-white/5">
        <div className="max-w-7xl mx-auto px-6 h-16 flex items-center justify-between">
          <div className="flex items-center gap-2 font-bold text-xl tracking-tighter">
            <span className="text-primary">DASH</span>
            <span className="text-gray-400 font-light">Algorithm</span>
          </div>

          <div className="hidden md:flex items-center gap-8">
            {navItems.map((item) => (
              <button
                key={item.id}
                onClick={() => {
                  setActiveSection(item.id);
                  document.getElementById(item.id)?.scrollIntoView({ behavior: 'smooth' });
                }}
                className={`text-sm font-medium transition-colors flex items-center gap-2 ${activeSection === item.id ? 'text-white' : 'text-gray-400 hover:text-white'
                  }`}
              >
                <item.icon size={16} />
                {item.label}
              </button>
            ))}
          </div>

          <a
            href="https://github.com/BHAWESHBHASKAR/DASH-Degree-Adaptive-Shortest-path-Heuristic-"
            target="_blank"
            rel="noopener noreferrer"
            className="p-2 hover:bg-white/10 rounded-full transition-colors"
          >
            <Github size={20} />
          </a>
        </div>
      </nav>

      {/* Main Content */}
      <main className="pt-16">
        <section id="hero">
          <Hero setActiveSection={setActiveSection} />
        </section>

        <section id="math" className="py-24 px-6 max-w-7xl mx-auto">
          <MathSection />
        </section>

        <section id="benchmarks" className="py-24 bg-surface/30">
          <div className="max-w-7xl mx-auto px-6">
            <Benchmarks />
          </div>
        </section>

        <section id="simulation" className="py-24 px-6 max-w-7xl mx-auto">
          <Simulation />
        </section>

        <section id="code" className="py-24 bg-surface/30">
          <div className="max-w-7xl mx-auto px-6">
            <CodeSection />
          </div>
        </section>
      </main>

      {/* Footer */}
      <footer className="py-12 border-t border-white/5 text-center text-gray-500 text-sm">
        <p>Advanced Agentic Coding Research • Google Deepmind</p>
        <p className="mt-2 text-xs">Developed by Antigravity</p>
      </footer>
    </div>
  );
}

export default App;
