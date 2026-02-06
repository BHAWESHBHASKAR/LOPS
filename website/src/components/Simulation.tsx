import React, { useEffect, useMemo, useRef, useState } from 'react';
import { Play, Pause, RefreshCw, SlidersHorizontal, ListChecks } from 'lucide-react';

const GRID_WIDTH = 32;
const GRID_HEIGHT = 20;
const GRID_SIZE = GRID_WIDTH * GRID_HEIGHT;

const GRAPH_DEFAULT_NODES = 140;

const DATASETS = [
  { id: 'snap-enron', name: 'SNAP Enron (sample)', type: 'snap', file: '/datasets/snap-enron.json' },
  { id: 'snap-epinions', name: 'SNAP Epinions (sample)', type: 'snap', file: '/datasets/snap-epinions.json' },
  { id: 'snap-wiki', name: 'SNAP Wiki-Vote (sample)', type: 'snap', file: '/datasets/snap-wiki.json' },
  { id: 'snap-facebook', name: 'SNAP Facebook (sample)', type: 'snap', file: '/datasets/snap-facebook.json' },
  { id: 'snap-grqc', name: 'SNAP CA-GrQc (sample)', type: 'snap', file: '/datasets/snap-grqc.json' },
  { id: 'synthetic', name: 'Synthetic (scale-free)', type: 'synthetic' },
] as const;

const ALGORITHMS = [
  {
    id: 'dijkstra',
    name: 'Dijkstra',
    mode: 'uni',
    heuristic: 'none',
    weight: 0,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'astar',
    name: 'A*',
    mode: 'uni',
    heuristic: 'geo',
    weight: 1,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'bidir',
    name: 'Bidirectional',
    mode: 'bidir',
    heuristic: 'none',
    weight: 0,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'delta',
    name: 'Delta-Stepping',
    mode: 'delta',
    heuristic: 'none',
    weight: 0,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'wavefront',
    name: 'Parallel Wavefront',
    mode: 'wavefront',
    heuristic: 'none',
    weight: 0,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'lips-exact',
    name: 'LIPS-Exact',
    mode: 'uni',
    heuristic: 'lips',
    weight: 1,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'lips-weighted',
    name: 'LIPS-Weighted',
    mode: 'uni',
    heuristic: 'lips',
    weight: 1.6,
    dashBias: 0,
    phase: 'Approx',
  },
  {
    id: 'lips-hybrid',
    name: 'LIPS-Hybrid',
    mode: 'hybrid',
    heuristic: 'lips',
    weight: 1.6,
    dashBias: 0,
    phase: 'Exact',
  },
  {
    id: 'dash-single',
    name: 'DASH-Single',
    mode: 'uni',
    heuristic: 'geo',
    weight: 1,
    dashBias: 1.3,
    phase: 'Exact',
  },
  {
    id: 'dash-bidir',
    name: 'DASH-Bidir',
    mode: 'bidir',
    heuristic: 'geo',
    weight: 1,
    dashBias: 1.3,
    phase: 'Exact',
  },
  {
    id: 'dash-auto',
    name: 'DASH-Auto',
    mode: 'auto',
    heuristic: 'geo',
    weight: 1,
    dashBias: 1.3,
    phase: 'Exact',
  },
  {
    id: 'photon',
    name: 'PHOTON Auto',
    mode: 'auto',
    heuristic: 'geo',
    weight: 1,
    dashBias: 0.8,
    phase: 'Exact',
  },
] as const;

type AlgorithmId = typeof ALGORITHMS[number]['id'];

type AlgorithmConfig = typeof ALGORITHMS[number];

type DatasetId = typeof DATASETS[number]['id'];

type GraphDataset = {
  name: string;
  nodeCount: number;
  edgeCount: number;
  source?: number;
  target?: number;
  nodes: { id: number; x: number; y: number; degree: number }[];
  edges: { s: number; t: number }[];
};

type RunResult = {
  id: AlgorithmId;
  name: string;
  phase: string;
  timeMs: number;
  steps: number;
  expansions: number;
  distance: number;
  speedup: number | null;
  optimality: number | null;
};

type AuditResult = {
  category: string;
  algorithm: string;
  phase: string;
  medianTime: number;
  medianExpansions: number;
  successRate: number;
  optimalityMedian: number | null;
};

const gridIndex = (x: number, y: number) => y * GRID_WIDTH + x;

const neighbors4 = (idx: number) => {
  const x = idx % GRID_WIDTH;
  const y = Math.floor(idx / GRID_WIDTH);
  const n: number[] = [];
  if (x > 0) n.push(idx - 1);
  if (x < GRID_WIDTH - 1) n.push(idx + 1);
  if (y > 0) n.push(idx - GRID_WIDTH);
  if (y < GRID_HEIGHT - 1) n.push(idx + GRID_WIDTH);
  return n;
};

const manhattan = (a: number, b: number) => {
  const ax = a % GRID_WIDTH;
  const ay = Math.floor(a / GRID_WIDTH);
  const bx = b % GRID_WIDTH;
  const by = Math.floor(b / GRID_WIDTH);
  return Math.abs(ax - bx) + Math.abs(ay - by);
};

const lerp = (a: number, b: number, t: number) => a + (b - a) * t;

const pick = <T,>(arr: T[]) => arr[Math.floor(Math.random() * arr.length)];

const useAlgorithmConfig = (id: AlgorithmId): AlgorithmConfig => {
  return useMemo(() => ALGORITHMS.find((algo) => algo.id === id)!, [id]);
};

export const Simulation = () => {
  const gridCanvas = useRef<HTMLCanvasElement>(null);
  const graphCanvas = useRef<HTMLCanvasElement>(null);
  const timelineCanvas = useRef<HTMLCanvasElement>(null);

  const [activeAlgorithm, setActiveAlgorithm] = useState<AlgorithmId>('dash-auto');
  const [activeDataset, setActiveDataset] = useState<DatasetId>('snap-enron');
  const [isRunning, setIsRunning] = useState(false);
  const [isBatchRunning, setIsBatchRunning] = useState(false);
  const [isAuditRunning, setIsAuditRunning] = useState(false);
  const [speed, setSpeed] = useState(48);
  const [stepCount, setStepCount] = useState(0);
  const [runResults, setRunResults] = useState<RunResult[]>([]);
  const [auditResults, setAuditResults] = useState<AuditResult[]>([]);
  const [batchCopied, setBatchCopied] = useState(false);
  const [auditCopied, setAuditCopied] = useState(false);
  const [singleResult, setSingleResult] = useState<RunResult | null>(null);
  const [datasetInfo, setDatasetInfo] = useState<GraphDataset | null>(null);
  const stepRef = useRef(0);

  const algo = useAlgorithmConfig(activeAlgorithm);

  const gridObstacles = useRef<Set<number>>(new Set());
  const gridSource = useRef(0);
  const gridTarget = useRef(GRID_SIZE - 1);
  const gridFrontier = useRef<number[]>([]);
  const gridVisited = useRef<Set<number>>(new Set());
  const gridG = useRef<Float32Array>(new Float32Array(GRID_SIZE));
  const gridPotentials = useRef<Float32Array[]>([]);

  const graphNodes = useRef<{ id: number; x: number; y: number; degree: number; hub: boolean }[]>([]);
  const graphEdges = useRef<{ s: number; t: number }[]>([]);
  const graphAdj = useRef<number[][]>([]);
  const graphFrontier = useRef<number[]>([]);
  const graphVisited = useRef<Set<number>>(new Set());
  const graphG = useRef<Float32Array>(new Float32Array(GRAPH_DEFAULT_NODES));
  const graphSource = useRef(0);
  const graphTarget = useRef(1);
  const graphPotentials = useRef<Float32Array[]>([]);

  const timeline = useRef<{ step: number; frontier: number; visited: number }[]>([]);

  const initGrid = () => {
    const obstacles = new Set<number>();
    for (let i = 0; i < GRID_SIZE; i++) {
      if (Math.random() < 0.18) obstacles.add(i);
    }

    const source = gridIndex(2, 2);
    const target = gridIndex(GRID_WIDTH - 3, GRID_HEIGHT - 3);
    obstacles.delete(source);
    obstacles.delete(target);

    gridObstacles.current = obstacles;
    gridSource.current = source;
    gridTarget.current = target;

    gridG.current = new Float32Array(GRID_SIZE).fill(Number.POSITIVE_INFINITY);
    gridG.current[source] = 0;
    gridFrontier.current = [source];
    gridVisited.current = new Set();

    gridPotentials.current = buildGridPotentials(obstacles);
  };

  const buildGridPotentials = (obstacles: Set<number>) => {
    const potentials: Float32Array[] = [];
    const anchorsPerPot = 4;
    const potCount = 4;
    const candidates = Array.from({ length: GRID_SIZE }, (_, i) => i).filter((i) => !obstacles.has(i));

    for (let p = 0; p < potCount; p++) {
      const anchors = Array.from({ length: anchorsPerPot }, () => pick(candidates));
      const labels = anchors.map(() => Math.random() * 6);
      const phi = new Float32Array(GRID_SIZE);
      for (let i = 0; i < GRID_SIZE; i++) {
        if (obstacles.has(i)) {
          phi[i] = Number.POSITIVE_INFINITY;
          continue;
        }
        let best = Number.POSITIVE_INFINITY;
        for (let a = 0; a < anchors.length; a++) {
          const val = labels[a] + manhattan(anchors[a], i);
          if (val < best) best = val;
        }
        phi[i] = best;
      }
      potentials.push(phi);
    }

    return potentials;
  };

  const initGraphSynthetic = () => {
    const width = 760;
    const height = 360;
    const hubs = [
      { x: width * 0.2, y: height * 0.3 },
      { x: width * 0.55, y: height * 0.7 },
      { x: width * 0.78, y: height * 0.35 },
    ];

    const nodes = Array.from({ length: GRAPH_DEFAULT_NODES }, (_, id) => {
      if (id < hubs.length) {
        return { id, x: hubs[id].x, y: hubs[id].y, degree: 0, hub: true };
      }
      const hub = pick(hubs);
      const angle = Math.random() * Math.PI * 2;
      const dist = Math.random() * 160;
      const x = Math.min(width - 20, Math.max(20, hub.x + Math.cos(angle) * dist));
      const y = Math.min(height - 20, Math.max(20, hub.y + Math.sin(angle) * dist));
      return { id, x, y, degree: 0, hub: false };
    });

    const edges: { s: number; t: number }[] = [];
    for (let i = 0; i < GRAPH_DEFAULT_NODES; i++) {
      for (let j = i + 1; j < GRAPH_DEFAULT_NODES; j++) {
        const dx = nodes[i].x - nodes[j].x;
        const dy = nodes[i].y - nodes[j].y;
        const dist = Math.hypot(dx, dy);
        const hubProb = nodes[i].hub || nodes[j].hub ? 0.32 : 0.05;
        if (dist < 80 || (dist < 180 && Math.random() < hubProb)) {
          edges.push({ s: i, t: j });
          nodes[i].degree++;
          nodes[j].degree++;
        }
      }
    }

    graphNodes.current = nodes;
    graphEdges.current = edges;
    graphAdj.current = buildAdjacency(nodes.length, edges);
    graphSource.current = 0;
    graphTarget.current = hubs.length - 1;

    resetGraphState();
    graphPotentials.current = buildGraphPotentials(nodes, graphAdj.current);
  };

  const initGraphFromDataset = (dataset: GraphDataset) => {
    const nodes = dataset.nodes.map((n) => ({ ...n, hub: false }));
    const edges = dataset.edges;
    const adj = buildAdjacency(nodes.length, edges);
    const maxDeg = Math.max(...nodes.map((n) => n.degree), 1);
    nodes.forEach((n) => {
      n.hub = n.degree >= maxDeg * 0.7;
    });

    graphNodes.current = nodes;
    graphEdges.current = edges;
    graphAdj.current = adj;
    graphSource.current = Math.min(dataset.source ?? 0, Math.max(0, nodes.length - 1));
    graphTarget.current = Math.min(dataset.target ?? Math.max(0, nodes.length - 1), Math.max(0, nodes.length - 1));

    resetGraphState();
    graphPotentials.current = buildGraphPotentials(nodes, graphAdj.current);
  };

  const buildAdjacency = (nodeCount: number, edges: { s: number; t: number }[]) => {
    const adj = Array.from({ length: nodeCount }, () => [] as number[]);
    edges.forEach((edge) => {
      adj[edge.s].push(edge.t);
      adj[edge.t].push(edge.s);
    });
    return adj;
  };

  const resetGridState = () => {
    const source = gridSource.current;
    gridG.current = new Float32Array(GRID_SIZE).fill(Number.POSITIVE_INFINITY);
    gridG.current[source] = 0;
    gridFrontier.current = [source];
    gridVisited.current = new Set();
  };

  const resetGraphState = () => {
    const nodeCount = graphNodes.current.length || GRAPH_DEFAULT_NODES;
    graphG.current = new Float32Array(nodeCount).fill(Number.POSITIVE_INFINITY);
    graphG.current[graphSource.current] = 0;
    graphFrontier.current = [graphSource.current];
    graphVisited.current = new Set();
  };

  const buildGraphPotentials = (
    nodes: { x: number; y: number; degree?: number }[],
    adj: number[][],
  ) => {
    const potentials: Float32Array[] = [];
    const potCount = 6;
    const anchorsPerPot = 6;
    const nodeCount = nodes.length;
    if (!nodeCount) return potentials;

    const indices = Array.from({ length: nodeCount }, (_, i) => i);
    const degrees = nodes.map((n) => n.degree ?? 0);
    const sortedByDegree = [...indices].sort((a, b) => degrees[a] - degrees[b]);
    const hubCount = Math.max(3, Math.floor(nodeCount * 0.1));
    const perCount = Math.max(3, Math.floor(nodeCount * 0.25));
    const hubs = sortedByDegree.slice(-hubCount);
    const periphery = sortedByDegree.slice(0, perCount);

    const pickRandom = (list: number[]) => list[Math.floor(Math.random() * list.length)];
    const bfsDistancesLocal = (source: number) => {
      const dist = new Int32Array(nodeCount).fill(-1);
      const queue: number[] = [];
      dist[source] = 0;
      queue.push(source);
      while (queue.length) {
        const node = queue.shift()!;
        const nextDist = dist[node] + 1;
        for (const nb of adj[node] ?? []) {
          if (dist[nb] !== -1) continue;
          dist[nb] = nextDist;
          queue.push(nb);
        }
      }
      return dist;
    };

    const farthestPair = (() => {
      const start = Math.floor(Math.random() * nodeCount);
      const distA = bfsDistancesLocal(start);
      let a = 0;
      let maxA = -1;
      distA.forEach((value, idx) => {
        if (value > maxA) {
          maxA = value;
          a = idx;
        }
      });
      const distB = bfsDistancesLocal(a);
      let b = a;
      let maxB = -1;
      distB.forEach((value, idx) => {
        if (value > maxB) {
          maxB = value;
          b = idx;
        }
      });
      return { source: a, target: b };
    })();

    const anchorSets: number[][] = [];
    for (let p = 0; p < potCount; p++) {
      const anchors = new Set<number>();
      if (p === 0 && farthestPair) {
        anchors.add(farthestPair.source);
        anchors.add(farthestPair.target);
      }
      if (p === 1) {
        while (anchors.size < Math.min(anchorsPerPot, hubs.length)) anchors.add(pickRandom(hubs));
      }
      if (p === 2) {
        while (anchors.size < Math.min(anchorsPerPot, periphery.length)) anchors.add(pickRandom(periphery));
      }
      if (p === 3) {
        anchors.add(pickRandom(hubs));
        anchors.add(pickRandom(periphery));
      }
      while (anchors.size < anchorsPerPot) {
        anchors.add(Math.floor(Math.random() * nodeCount));
      }
      anchorSets.push(Array.from(anchors));
    }

    const distanceCache = new Map<number, Int32Array>();
    const getDistances = (anchor: number) => {
      if (!distanceCache.has(anchor)) {
        distanceCache.set(anchor, bfsDistancesLocal(anchor));
      }
      return distanceCache.get(anchor)!;
    };

    anchorSets.forEach((anchors) => {
      const labels = anchors.map(() => Math.random() * 5);
      const phi = new Float32Array(nodeCount);
      for (let i = 0; i < nodeCount; i++) {
        let best = Number.POSITIVE_INFINITY;
        for (let a = 0; a < anchors.length; a++) {
          const dist = getDistances(anchors[a])[i];
          if (dist === -1) continue;
          const val = labels[a] + dist;
          if (val < best) best = val;
        }
        phi[i] = best;
      }
      potentials.push(phi);
    });

    return potentials;
  };

  const resetAll = (reseed = true) => {
    if (reseed) {
      initGrid();
    } else {
      resetGridState();
    }

    if (reseed) {
      if (activeDataset === 'synthetic') {
        initGraphSynthetic();
      } else if (datasetInfo) {
        initGraphFromDataset(datasetInfo);
      } else {
        initGraphSynthetic();
      }
    } else {
      resetGraphState();
    }

    timeline.current = [];
    setStepCount(0);
    stepRef.current = 0;
    setIsRunning(false);
    drawAll();
  };

  const loadDataset = async (datasetId: DatasetId) => {
    const meta = DATASETS.find((item) => item.id === datasetId);
    if (!meta) return;

    setIsRunning(false);
    setIsBatchRunning(false);
    setIsAuditRunning(false);
    setRunResults([]);
    setAuditResults([]);
    setSingleResult(null);
    timeline.current = [];
    stepRef.current = 0;
    setStepCount(0);

    initGrid();

    if (meta.type === 'synthetic') {
      setDatasetInfo(null);
      initGraphSynthetic();
      drawAll();
      return;
    }

    try {
      const response = await fetch(meta.file!);
      const data = (await response.json()) as GraphDataset;
      setDatasetInfo(data);
      initGraphFromDataset(data);
      drawAll();
    } catch (error) {
      console.error('Failed to load dataset', error);
    }
  };

  const resetSearchState = () => {
    resetGridState();
    resetGraphState();
    timeline.current = [];
    stepRef.current = 0;
    setStepCount(0);
  };

  const pickFreeCell = () => {
    const attempts = 2000;
    for (let i = 0; i < attempts; i++) {
      const idx = Math.floor(Math.random() * GRID_SIZE);
      if (!gridObstacles.current.has(idx)) return idx;
    }
    return gridIndex(1, 1);
  };

  const bfsDistances = (adj: number[][], source: number) => {
    const dist = new Int32Array(adj.length).fill(-1);
    const queue: number[] = [];
    dist[source] = 0;
    queue.push(source);
    while (queue.length) {
      const node = queue.shift()!;
      const nextDist = dist[node] + 1;
      for (const nb of adj[node] ?? []) {
        if (dist[nb] !== -1) continue;
        dist[nb] = nextDist;
        queue.push(nb);
      }
    }
    return dist;
  };

  const farthestFrom = (dist: Int32Array) => {
    let maxIdx = 0;
    let maxDist = -1;
    dist.forEach((value, idx) => {
      if (value > maxDist) {
        maxDist = value;
        maxIdx = idx;
      }
    });
    return { node: maxIdx, dist: maxDist };
  };

  const pickFarthestPair = () => {
    const adj = graphAdj.current;
    if (!adj.length) return null;
    const start = Math.floor(Math.random() * adj.length);
    const distA = bfsDistances(adj, start);
    const a = farthestFrom(distA).node;
    const distB = bfsDistances(adj, a);
    const b = farthestFrom(distB).node;
    return { source: a, target: b, distance: distB[b] };
  };

  const randomizeEndpoints = () => {
    const source = pickFreeCell();
    let target = pickFreeCell();
    let guard = 0;
    while (target === source && guard < 50) {
      target = pickFreeCell();
      guard += 1;
    }
    gridSource.current = source;
    gridTarget.current = target;

    const pair = pickFarthestPair();
    if (pair) {
      graphSource.current = pair.source;
      graphTarget.current = pair.target;
    }

    resetSearchState();
    drawAll();
  };

  const lipsHeuristicGrid = (node: number) => {
    const target = gridTarget.current;
    let h = 0;
    gridPotentials.current.forEach((phi) => {
      const diff = Math.abs(phi[node] - phi[target]);
      if (diff > h) h = diff;
    });
    return h;
  };

  const lipsHeuristicGraph = (node: number) => {
    const target = graphTarget.current;
    if (graphPotentials.current.length === 0) return 0;
    let h = 0;
    graphPotentials.current.forEach((phi) => {
      const diff = Math.abs(phi[node] - phi[target]);
      if (diff > h) h = diff;
    });
    return h;
  };

  const geoHeuristicGrid = (node: number) => manhattan(node, gridTarget.current);

  const geoHeuristicGraph = (node: number) => {
    const a = graphNodes.current[node];
    const b = graphNodes.current[graphTarget.current];
    if (!a || !b) return 0;
    return Math.hypot(a.x - b.x, a.y - b.y) / 40;
  };

  const dashBiasGrid = (node: number) => {
    const deg = neighbors4(node).filter((n) => !gridObstacles.current.has(n)).length;
    return deg / 4;
  };

  const dashBiasGraph = (node: number) => {
    const nodes = graphNodes.current;
    if (nodes.length === 0) return 0;
    const maxDeg = Math.max(...nodes.map((n) => n.degree), 1);
    const deg = nodes[node]?.degree ?? 0;
    return Math.log2(deg + 1) / Math.log2(maxDeg + 1);
  };

  const scoreNode = (node: number, g: number, heuristic: number, dashBias: number, weight: number) => {
    return g + weight * heuristic - dashBias;
  };

  const stepGrid = (config: AlgorithmConfig) => {
    if (gridFrontier.current.length === 0) return;

    const weight = config.id === 'lips-hybrid' && stepRef.current > 30 ? 1.0 : config.weight;
    const useBidir = config.mode === 'bidir';
    const useWavefront = config.mode === 'wavefront';
    const useDelta = config.mode === 'delta';

    const frontier = gridFrontier.current;
    const visited = gridVisited.current;
    const gScores = gridG.current;
    const target = gridTarget.current;

    const expand = (node: number) => {
      neighbors4(node).forEach((n) => {
        if (gridObstacles.current.has(n) || visited.has(n)) return;
        const ng = gScores[node] + 1;
        if (ng < gScores[n]) {
          gScores[n] = ng;
          if (!frontier.includes(n)) frontier.push(n);
        }
      });
    };

    if (useWavefront) {
      const batch = [...frontier];
      frontier.length = 0;
      batch.forEach((node) => {
        visited.add(node);
        expand(node);
      });
      return;
    }

    if (useDelta) {
      const minG = Math.min(...frontier.map((n) => gScores[n]));
      const delta = 2;
      const batch = frontier.filter((n) => gScores[n] <= minG + delta);
      gridFrontier.current = frontier.filter((n) => !batch.includes(n));
      batch.forEach((node) => {
        visited.add(node);
        expand(node);
      });
      return;
    }

    if (useBidir) {
      const node = frontier.shift()!;
      visited.add(node);
      expand(node);
      if (node === target) frontier.length = 0;
      return;
    }

    frontier.sort((a, b) => {
      const ha = config.heuristic === 'lips' ? lipsHeuristicGrid(a) : config.heuristic === 'geo' ? geoHeuristicGrid(a) : 0;
      const hb = config.heuristic === 'lips' ? lipsHeuristicGrid(b) : config.heuristic === 'geo' ? geoHeuristicGrid(b) : 0;
      const ba = config.dashBias ? dashBiasGrid(a) * config.dashBias : 0;
      const bb = config.dashBias ? dashBiasGrid(b) * config.dashBias : 0;
      const sa = scoreNode(a, gScores[a], ha, ba, weight);
      const sb = scoreNode(b, gScores[b], hb, bb, weight);
      return sa - sb;
    });

    const current = frontier.shift()!;
    visited.add(current);
    if (current === target) {
      frontier.length = 0;
      return;
    }
    expand(current);
  };

  const stepGraph = (config: AlgorithmConfig) => {
    if (graphFrontier.current.length === 0) return;

    const weight = config.id === 'lips-hybrid' && stepRef.current > 30 ? 1.0 : config.weight;
    const useWavefront = config.mode === 'wavefront';
    const useDelta = config.mode === 'delta';

    const frontier = graphFrontier.current;
    const visited = graphVisited.current;
    const gScores = graphG.current;
    const target = graphTarget.current;

    const neighbors = (node: number) => graphAdj.current[node] ?? [];

    const expand = (node: number) => {
      neighbors(node).forEach((n) => {
        if (visited.has(n)) return;
        const ng = gScores[node] + 1;
        if (ng < gScores[n]) {
          gScores[n] = ng;
          if (!frontier.includes(n)) frontier.push(n);
        }
      });
    };

    if (useWavefront) {
      const batch = [...frontier];
      frontier.length = 0;
      batch.forEach((node) => {
        visited.add(node);
        expand(node);
      });
      return;
    }

    if (useDelta) {
      const minG = Math.min(...frontier.map((n) => gScores[n]));
      const delta = 2;
      const batch = frontier.filter((n) => gScores[n] <= minG + delta);
      graphFrontier.current = frontier.filter((n) => !batch.includes(n));
      batch.forEach((node) => {
        visited.add(node);
        expand(node);
      });
      return;
    }

    frontier.sort((a, b) => {
      const ha = config.heuristic === 'lips' ? lipsHeuristicGraph(a) : config.heuristic === 'geo' ? geoHeuristicGraph(a) : 0;
      const hb = config.heuristic === 'lips' ? lipsHeuristicGraph(b) : config.heuristic === 'geo' ? geoHeuristicGraph(b) : 0;
      const ba = config.dashBias ? dashBiasGraph(a) * config.dashBias : 0;
      const bb = config.dashBias ? dashBiasGraph(b) * config.dashBias : 0;
      const sa = scoreNode(a, gScores[a], ha, ba, weight);
      const sb = scoreNode(b, gScores[b], hb, bb, weight);
      return sa - sb;
    });

    const current = frontier.shift()!;
    visited.add(current);
    if (current === target) {
      frontier.length = 0;
      return;
    }
    expand(current);
  };

  const tick = () => {
    const config = algo.mode === 'auto'
      ? activeAlgorithm === 'dash-auto'
        ? { ...algo, mode: 'bidir', dashBias: 1.3, heuristic: 'geo', weight: 1 }
        : { ...algo, mode: 'uni', heuristic: 'geo', weight: 1 }
      : algo;

    stepGrid(config);
    stepGraph(config);

    timeline.current.push({
      step: stepRef.current,
      frontier: gridFrontier.current.length,
      visited: gridVisited.current.size,
    });

    stepRef.current += 1;
    setStepCount(stepRef.current);
    drawAll();
  };

  const runtimeConfig = (config: AlgorithmConfig, step: number) => {
    if (config.mode === 'auto') {
      if (config.id === 'dash-auto') {
        return { ...config, mode: 'bidir', dashBias: 1.3, heuristic: 'geo', weight: 1 };
      }
      return { ...config, mode: 'uni', heuristic: 'geo', weight: 1 };
    }
    if (config.id === 'lips-hybrid') {
      return { ...config, weight: step > 30 ? 1.0 : config.weight };
    }
    return config;
  };

  const runGraphAlgorithm = (config: AlgorithmConfig, source: number, target: number) => {
    graphSource.current = source;
    graphTarget.current = target;
    resetGraphState();

    const start = performance.now();
    let steps = 0;
    const maxSteps = Math.max(2000, graphNodes.current.length * 30);

    while (graphFrontier.current.length > 0 && steps < maxSteps) {
      const currentConfig = runtimeConfig(config, steps);
      stepGraph(currentConfig);
      steps += 1;
    }

    const timeMs = performance.now() - start;
    const distance = graphG.current[graphTarget.current];
    const expansions = graphVisited.current.size;
    return { timeMs, steps, distance, expansions };
  };

  const runAllAlgorithms = async () => {
    if (isBatchRunning) return;
    setIsBatchRunning(true);
    setRunResults([]);
    randomizeEndpoints();

    const baselineConfig = ALGORITHMS.find((algo) => algo.id === 'dijkstra')!;
    const source = graphSource.current;
    const target = graphTarget.current;
    const baseline = runGraphAlgorithm(baselineConfig, source, target);
    const baselineDistance = baseline.distance;
    const baselineTime = Math.max(baseline.timeMs, 0.01);

    const results: RunResult[] = [
      {
        id: baselineConfig.id,
        name: baselineConfig.name,
        phase: baselineConfig.phase,
        timeMs: baseline.timeMs,
        steps: baseline.steps,
        expansions: baseline.expansions,
        distance: baseline.distance,
        speedup: 1,
        optimality: 100,
      },
    ];

    for (const algoConfig of ALGORITHMS) {
      if (algoConfig.id === 'dijkstra') continue;
      // Let the UI breathe between runs.
      await new Promise((resolve) => setTimeout(resolve, 0));
      const run = runGraphAlgorithm(algoConfig, source, target);
      const speedup = baselineTime > 0 ? baselineTime / Math.max(run.timeMs, 0.01) : null;
      const optimality = Number.isFinite(baselineDistance) && Number.isFinite(run.distance)
        ? Math.min(100, (baselineDistance / run.distance) * 100)
        : null;

      results.push({
        id: algoConfig.id,
        name: algoConfig.name,
        phase: algoConfig.phase,
        timeMs: run.timeMs,
        steps: run.steps,
        expansions: run.expansions,
        distance: run.distance,
        speedup,
        optimality,
      });
    }

    setRunResults(results);
    setIsBatchRunning(false);
    resetSearchState();
    drawAll();
  };

  const median = (values: number[]) => {
    if (!values.length) return 0;
    const sorted = [...values].sort((a, b) => a - b);
    const mid = Math.floor(sorted.length / 2);
    if (sorted.length % 2 === 0) {
      return (sorted[mid - 1] + sorted[mid]) / 2;
    }
    return sorted[mid];
  };

  const pickPairs = (category: string, count: number) => {
    const nodeCount = graphNodes.current.length;
    if (nodeCount === 0) return [] as { source: number; target: number }[];
    const degrees = graphNodes.current.map((n) => n.degree);
    const indices = Array.from({ length: nodeCount }, (_, i) => i);
    const sorted = [...indices].sort((a, b) => degrees[a] - degrees[b]);
    const hubs = sorted.slice(-Math.max(3, Math.floor(nodeCount * 0.1)));
    const peripheral = sorted.slice(0, Math.max(3, Math.floor(nodeCount * 0.25)));

    const pairs: { source: number; target: number }[] = [];
    const addPair = (source: number, target: number) => {
      if (source === target) return;
      pairs.push({ source, target });
    };

    if (category === 'diameter') {
      const pair = pickFarthestPair();
      if (pair) addPair(pair.source, pair.target);
      return pairs;
    }

    if (category === 'hub-peripheral') {
      for (let i = 0; i < count; i++) {
        addPair(hubs[Math.floor(Math.random() * hubs.length)], peripheral[Math.floor(Math.random() * peripheral.length)]);
      }
      return pairs;
    }

    if (category === 'peripheral') {
      for (let i = 0; i < count; i++) {
        addPair(peripheral[Math.floor(Math.random() * peripheral.length)], peripheral[Math.floor(Math.random() * peripheral.length)]);
      }
      return pairs;
    }

    for (let i = 0; i < count; i++) {
      addPair(Math.floor(Math.random() * nodeCount), Math.floor(Math.random() * nodeCount));
    }
    return pairs;
  };

  const runAudit = async () => {
    if (isAuditRunning) return;
    setIsAuditRunning(true);
    setAuditResults([]);
    randomizeEndpoints();

    const categories = [
      { id: 'random', name: 'Random pairs', count: 6 },
      { id: 'diameter', name: 'Diameter', count: 1 },
      { id: 'hub-peripheral', name: 'Hub → Peripheral', count: 6 },
      { id: 'peripheral', name: 'Peripheral ↔ Peripheral', count: 6 },
    ];

    const results: AuditResult[] = [];
    for (const category of categories) {
      const pairs = pickPairs(category.id, category.count);
      if (!pairs.length) continue;

      for (const algoConfig of ALGORITHMS) {
        const times: number[] = [];
        const expansions: number[] = [];
        const optimalities: number[] = [];
        let success = 0;

        for (const pair of pairs) {
          await new Promise((resolve) => setTimeout(resolve, 0));
          const baseline = runGraphAlgorithm(ALGORITHMS[0], pair.source, pair.target);
          const run = algoConfig.id === 'dijkstra'
            ? baseline
            : runGraphAlgorithm(algoConfig, pair.source, pair.target);

          const baselineDist = baseline.distance;
          const runDist = run.distance;
          const ok = Number.isFinite(baselineDist) && Number.isFinite(runDist);
          if (ok) {
            success += 1;
            optimalities.push(Math.min(100, (baselineDist / runDist) * 100));
          }

          times.push(run.timeMs);
          expansions.push(run.expansions);
        }

        results.push({
          category: category.name,
          algorithm: algoConfig.name,
          phase: algoConfig.phase,
          medianTime: median(times),
          medianExpansions: median(expansions),
          successRate: (success / pairs.length) * 100,
          optimalityMedian: optimalities.length ? median(optimalities) : null,
        });
      }
    }

    setAuditResults(results);
    setIsAuditRunning(false);
    resetSearchState();
    drawAll();
  };

  const copyToClipboard = async (text: string) => {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch (err) {
      const textarea = document.createElement('textarea');
      textarea.value = text;
      textarea.style.position = 'fixed';
      textarea.style.left = '-9999px';
      document.body.appendChild(textarea);
      textarea.select();
      try {
        document.execCommand('copy');
        document.body.removeChild(textarea);
        return true;
      } catch (copyErr) {
        document.body.removeChild(textarea);
        console.error('Copy failed', copyErr);
        return false;
      }
    }
  };

  const handleCopyBatch = async () => {
    if (!runResults.length) return;
    const header = ['Algorithm', 'Phase', 'Time (ms)', 'Expansions', 'Distance', 'Speedup', 'Optimality'];
    const rows = runResults.map((row) => [
      row.name,
      row.phase,
      row.timeMs.toFixed(2),
      row.expansions.toString(),
      Number.isFinite(row.distance) ? row.distance.toFixed(2) : 'inf',
      row.speedup ? row.speedup.toFixed(2) : '',
      row.optimality !== null ? row.optimality.toFixed(1) : '',
    ]);
    const text = [header, ...rows].map((line) => line.join('\t')).join('\n');
    const ok = await copyToClipboard(text);
    if (ok) {
      setBatchCopied(true);
      setTimeout(() => setBatchCopied(false), 1600);
    }
  };

  const handleCopyAudit = async () => {
    if (!auditResults.length) return;
    const header = ['Category', 'Algorithm', 'Phase', 'Median Time (ms)', 'Median Exp', 'Success %', 'Optimality %'];
    const rows = auditResults.map((row) => [
      row.category,
      row.algorithm,
      row.phase,
      row.medianTime.toFixed(2),
      row.medianExpansions.toFixed(0),
      row.successRate.toFixed(0),
      row.optimalityMedian !== null ? row.optimalityMedian.toFixed(1) : '',
    ]);
    const text = [header, ...rows].map((line) => line.join('\t')).join('\n');
    const ok = await copyToClipboard(text);
    if (ok) {
      setAuditCopied(true);
      setTimeout(() => setAuditCopied(false), 1600);
    }
  };

  const runSelectedAlgorithm = async () => {
    const source = graphSource.current;
    const target = graphTarget.current;
    const baselineConfig = ALGORITHMS.find((algo) => algo.id === 'dijkstra')!;
    const baseline = runGraphAlgorithm(baselineConfig, source, target);
    const activeConfig = ALGORITHMS.find((algo) => algo.id === activeAlgorithm)!;
    const run = activeConfig.id === 'dijkstra' ? baseline : runGraphAlgorithm(activeConfig, source, target);

    const baselineTime = Math.max(baseline.timeMs, 0.01);
    const speedup = baselineTime / Math.max(run.timeMs, 0.01);
    const optimality = Number.isFinite(baseline.distance) && Number.isFinite(run.distance)
      ? Math.min(100, (baseline.distance / run.distance) * 100)
      : null;

    setSingleResult({
      id: activeConfig.id,
      name: activeConfig.name,
      phase: activeConfig.phase,
      timeMs: run.timeMs,
      steps: run.steps,
      expansions: run.expansions,
      distance: run.distance,
      speedup,
      optimality,
    });
  };

  const drawGrid = () => {
    const ctx = gridCanvas.current?.getContext('2d');
    if (!ctx) return;

    const width = 640;
    const height = 360;
    ctx.clearRect(0, 0, width, height);

    const cellW = width / GRID_WIDTH;
    const cellH = height / GRID_HEIGHT;

    for (let y = 0; y < GRID_HEIGHT; y++) {
      for (let x = 0; x < GRID_WIDTH; x++) {
        const idx = gridIndex(x, y);
        if (gridObstacles.current.has(idx)) {
          ctx.fillStyle = '#1f2937';
        } else if (idx === gridSource.current) {
          ctx.fillStyle = '#22c55e';
        } else if (idx === gridTarget.current) {
          ctx.fillStyle = '#f97316';
        } else if (gridVisited.current.has(idx)) {
          ctx.fillStyle = '#0ea5e9';
        } else if (gridFrontier.current.includes(idx)) {
          ctx.fillStyle = '#facc15';
        } else {
          ctx.fillStyle = '#111827';
        }
        ctx.fillRect(x * cellW, y * cellH, cellW - 1, cellH - 1);
      }
    }
  };

  const drawGraph = () => {
    const ctx = graphCanvas.current?.getContext('2d');
    if (!ctx) return;

    const width = 760;
    const height = 360;
    ctx.clearRect(0, 0, width, height);

    ctx.strokeStyle = '#1f2937';
    ctx.lineWidth = 1;
    graphEdges.current.forEach((e) => {
      const s = graphNodes.current[e.s];
      const t = graphNodes.current[e.t];
      ctx.beginPath();
      ctx.moveTo(s.x, s.y);
      ctx.lineTo(t.x, t.y);
      ctx.stroke();
    });

    graphNodes.current.forEach((n) => {
      ctx.beginPath();
      ctx.arc(n.x, n.y, n.hub ? 7 : 3, 0, Math.PI * 2);
      if (n.id === graphSource.current) {
        ctx.fillStyle = '#22c55e';
      } else if (n.id === graphTarget.current) {
        ctx.fillStyle = '#f97316';
      } else if (graphVisited.current.has(n.id)) {
        ctx.fillStyle = '#38bdf8';
      } else if (graphFrontier.current.includes(n.id)) {
        ctx.fillStyle = '#facc15';
      } else {
        ctx.fillStyle = n.hub ? '#a3a3a3' : '#52525b';
      }
      ctx.fill();
    });
  };

  const drawTimeline = () => {
    const ctx = timelineCanvas.current?.getContext('2d');
    if (!ctx) return;

    const width = 760;
    const height = 180;
    ctx.clearRect(0, 0, width, height);

    const data = timeline.current.slice(-80);
    if (data.length === 0) return;

    const maxFrontier = Math.max(...data.map((d) => d.frontier), 1);
    const maxVisited = Math.max(...data.map((d) => d.visited), 1);

    const stepW = width / data.length;
    data.forEach((d, i) => {
      const frontierH = (d.frontier / maxFrontier) * (height * 0.5);
      const visitedH = (d.visited / maxVisited) * (height * 0.45);

      ctx.fillStyle = 'rgba(250, 204, 21, 0.7)';
      ctx.fillRect(i * stepW, height - frontierH, stepW - 2, frontierH);

      ctx.fillStyle = 'rgba(14, 165, 233, 0.5)';
      ctx.fillRect(i * stepW, height - frontierH - visitedH, stepW - 2, visitedH);
    });

    ctx.fillStyle = '#e2e8f0';
    ctx.font = '12px Space Grotesk, sans-serif';
    ctx.fillText('Frontier (gold) + Visited (blue)', 12, 18);
  };

  const drawAll = () => {
    drawGrid();
    drawGraph();
    drawTimeline();
  };

  useEffect(() => {
    loadDataset(activeDataset);
  }, [activeDataset]);

  useEffect(() => {
    let timer: number | undefined;
    if (isRunning) {
      timer = window.setInterval(tick, speed);
    }
    return () => {
      if (timer) window.clearInterval(timer);
    };
  }, [isRunning, speed, activeAlgorithm]);

  useEffect(() => {
    drawAll();
  }, [activeAlgorithm]);

  const algorithmOptions = useMemo(() => ALGORITHMS, []);
  const datasetOptions = useMemo(() => DATASETS, []);

  return (
    <section className="relative">
      <div className="absolute inset-0 opacity-20 pointer-events-none bg-[radial-gradient(circle_at_top,_rgba(15,118,110,0.3),transparent_55%)]" />

      <div className="relative grid gap-10">
        <div className="flex flex-col md:flex-row md:items-end md:justify-between gap-6">
          <div className="space-y-4">
            <p className="text-sm uppercase tracking-[0.2em] text-amber-400">Visualizer Suite</p>
            <h2 className="text-4xl md:text-5xl font-display font-semibold">Multiple lenses on every algorithm.</h2>
            <p className="text-slate-300 max-w-2xl">
              Load real-world SNAP samples and compare how each algorithm expands the frontier on a structured grid,
              a scale-free network, and a live expansion timeline.
            </p>
          </div>

          <div className="flex flex-wrap items-center gap-4 bg-surface/80 border border-white/10 rounded-2xl p-4">
            <div className="flex items-center gap-2 text-xs uppercase tracking-[0.2em] text-slate-400">
              <SlidersHorizontal size={16} />
              Dataset
            </div>
            <select
              value={activeDataset}
              onChange={(e) => setActiveDataset(e.target.value as DatasetId)}
              className="control-select"
            >
              {datasetOptions.map((option) => (
                <option key={option.id} value={option.id}>
                  {option.name}
                </option>
              ))}
            </select>

            <div className="flex items-center gap-2 text-xs uppercase tracking-[0.2em] text-slate-400">
              <SlidersHorizontal size={16} />
              Algorithm
            </div>
            <select
              value={activeAlgorithm}
              onChange={(e) => {
                setActiveAlgorithm(e.target.value as AlgorithmId);
                stepRef.current = 0;
                setStepCount(0);
                resetAll(false);
              }}
              className="control-select"
            >
              {algorithmOptions.map((option) => (
                <option key={option.id} value={option.id}>
                  {option.name}
                </option>
              ))}
            </select>
            <div className="text-xs text-slate-400">{algo.phase}</div>
          </div>
        </div>

        <div className="grid xl:grid-cols-[1.1fr_1.2fr] gap-8">
          <div className="bg-surface/70 border border-white/10 rounded-2xl p-6 shadow-[0_20px_60px_rgba(0,0,0,0.25)]">
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-lg font-semibold">Structured Grid Explorer</h3>
              <span className="text-xs uppercase text-slate-400">Geometry</span>
            </div>
            <canvas ref={gridCanvas} width={640} height={360} className="w-full rounded-xl border border-white/10 bg-black/30" />
          </div>

          <div className="bg-surface/70 border border-white/10 rounded-2xl p-6 shadow-[0_20px_60px_rgba(0,0,0,0.25)]">
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-lg font-semibold">Scale-Free Network Explorer</h3>
              <span className="text-xs uppercase text-slate-400">Topology</span>
            </div>
            <canvas ref={graphCanvas} width={760} height={360} className="w-full rounded-xl border border-white/10 bg-black/30" />
          </div>
        </div>

        <div className="grid lg:grid-cols-[1.2fr_0.8fr] gap-8">
          <div className="bg-surface/70 border border-white/10 rounded-2xl p-6">
            <div className="flex items-center justify-between mb-4">
              <h3 className="text-lg font-semibold">Frontier Timeline</h3>
              <span className="text-xs uppercase text-slate-400">Expansion Signal</span>
            </div>
            <canvas ref={timelineCanvas} width={760} height={180} className="w-full rounded-xl border border-white/10 bg-black/30" />
          </div>

          <div className="bg-surface/70 border border-white/10 rounded-2xl p-6 space-y-6">
            <div className="text-sm text-slate-300 space-y-2">
              <p className="text-slate-400">Active algorithm</p>
              <p className="text-xl font-semibold">{algo.name}</p>
              <p className="text-sm text-slate-400">Mode: {algo.mode} • Heuristic: {algo.heuristic}</p>
            </div>

            <div className="text-sm text-slate-300 space-y-1">
              <p className="text-slate-400">Dataset</p>
              <p className="text-base font-semibold">{datasetInfo?.name ?? 'Synthetic scale-free'}</p>
              <p className="text-xs text-slate-500">
                {datasetInfo
                  ? `${datasetInfo.nodeCount} nodes • ${datasetInfo.edgeCount} edges`
                  : `${GRAPH_DEFAULT_NODES} nodes • generated edges`}
              </p>
              <p className="text-xs text-slate-500">Real-life datasets are sampled for interactive speed.</p>
            </div>

            <div className="space-y-3">
              <label className="text-xs uppercase tracking-[0.2em] text-slate-400">Speed</label>
              <input
                type="range"
                min={24}
                max={120}
                value={speed}
                onChange={(e) => setSpeed(Number(e.target.value))}
                className="control-range"
              />
              <div className="text-xs text-slate-500">Step delay: {speed} ms</div>
            </div>

            <div className="flex gap-3">
              <button
                onClick={() => setIsRunning((prev) => !prev)}
                className="flex-1 flex items-center justify-center gap-2 px-4 py-3 rounded-lg bg-primary text-black font-semibold"
              >
                {isRunning ? <Pause size={16} /> : <Play size={16} />}
                {isRunning ? 'Pause' : 'Play'}
              </button>
              <button
                onClick={resetAll}
                className="flex-1 flex items-center justify-center gap-2 px-4 py-3 rounded-lg border border-white/10 text-white"
              >
                <RefreshCw size={16} />
                Reset
              </button>
            </div>

            <button
              onClick={randomizeEndpoints}
              className="w-full flex items-center justify-center gap-2 px-4 py-3 rounded-lg border border-white/10 text-white hover:bg-white/10 transition"
            >
              New endpoints
            </button>

            <button
              onClick={runAllAlgorithms}
              disabled={isBatchRunning}
              className="w-full flex items-center justify-center gap-2 px-4 py-3 rounded-lg border border-white/10 text-white hover:bg-white/10 transition disabled:opacity-60"
            >
              <ListChecks size={16} />
              {isBatchRunning ? 'Running all algorithms…' : 'Run All (Batch)'}
            </button>

            <button
              onClick={runSelectedAlgorithm}
              className="w-full flex items-center justify-center gap-2 px-4 py-3 rounded-lg border border-teal-400/40 text-teal-200 hover:bg-teal-400/10 transition"
            >
              Run Selected Algorithm
            </button>

            <button
              onClick={runAudit}
              disabled={isAuditRunning}
              className="w-full flex items-center justify-center gap-2 px-4 py-3 rounded-lg border border-amber-400/40 text-amber-200 hover:bg-amber-400/10 transition disabled:opacity-60"
            >
              {isAuditRunning ? 'Running audit…' : 'Run Audit (Expose Issues)'}
            </button>
          </div>
        </div>

        {singleResult && (
          <div className="bg-surface/70 border border-teal-400/20 rounded-2xl p-6">
            <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4 mb-4">
              <div>
                <h3 className="text-lg font-semibold">Selected Algorithm Result</h3>
                <p className="text-sm text-slate-400">
                  {datasetInfo?.name ?? 'Synthetic scale-free'} • endpoints preserved
                </p>
              </div>
              <div className="text-xs uppercase tracking-[0.2em] text-slate-500">
                Speedup vs Dijkstra • Optimality vs Dijkstra
              </div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-sm text-left">
                <thead className="text-slate-400 border-b border-white/10">
                  <tr>
                    <th className="py-2 pr-4 font-medium">Algorithm</th>
                    <th className="py-2 pr-4 font-medium">Phase</th>
                    <th className="py-2 pr-4 font-medium">Time (ms)</th>
                    <th className="py-2 pr-4 font-medium">Expansions</th>
                    <th className="py-2 pr-4 font-medium">Distance</th>
                    <th className="py-2 pr-4 font-medium">Speedup</th>
                    <th className="py-2 pr-4 font-medium">Optimality</th>
                  </tr>
                </thead>
                <tbody>
                  <tr className="border-b border-white/5">
                    <td className="py-3 pr-4 font-semibold text-white">{singleResult.name}</td>
                    <td className="py-3 pr-4 text-slate-300">{singleResult.phase}</td>
                    <td className="py-3 pr-4 text-slate-300">{singleResult.timeMs.toFixed(2)}</td>
                    <td className="py-3 pr-4 text-slate-300">{singleResult.expansions}</td>
                    <td className="py-3 pr-4 text-slate-300">
                      {Number.isFinite(singleResult.distance) ? singleResult.distance.toFixed(2) : '∞'}
                    </td>
                    <td className="py-3 pr-4 text-slate-300">{singleResult.speedup.toFixed(2)}×</td>
                    <td className="py-3 pr-4 text-slate-300">
                      {singleResult.optimality !== null ? singleResult.optimality.toFixed(1) + '%' : '—'}
                    </td>
                  </tr>
                </tbody>
              </table>
            </div>
          </div>
        )}

        {runResults.length > 0 && (
          <div className="bg-surface/70 border border-white/10 rounded-2xl p-6">
            <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4 mb-4">
              <div>
                <h3 className="text-lg font-semibold">Batch Results (Real-life Dataset)</h3>
                <p className="text-sm text-slate-400">
                  {datasetInfo?.name ?? 'Synthetic scale-free'} • {runResults.length} algorithms • endpoints randomized
                </p>
              </div>
              <div className="flex items-center gap-3">
                <div className="text-xs uppercase tracking-[0.2em] text-slate-500">
                  Speedup vs Dijkstra • Optimality vs Dijkstra
                </div>
                <button
                  onClick={handleCopyBatch}
                  className="text-xs uppercase tracking-[0.2em] px-3 py-2 rounded-lg border border-white/10 text-white hover:bg-white/10 transition"
                >
                  {batchCopied ? 'Copied' : 'Copy Table'}
                </button>
              </div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-sm text-left">
                <thead className="text-slate-400 border-b border-white/10">
                  <tr>
                    <th className="py-2 pr-4 font-medium">Algorithm</th>
                    <th className="py-2 pr-4 font-medium">Phase</th>
                    <th className="py-2 pr-4 font-medium">Time (ms)</th>
                    <th className="py-2 pr-4 font-medium">Expansions</th>
                    <th className="py-2 pr-4 font-medium">Distance</th>
                    <th className="py-2 pr-4 font-medium">Speedup</th>
                    <th className="py-2 pr-4 font-medium">Optimality</th>
                  </tr>
                </thead>
                <tbody>
                  {runResults.map((result) => (
                    <tr key={result.id} className="border-b border-white/5 last:border-0">
                      <td className="py-3 pr-4 font-semibold text-white">{result.name}</td>
                      <td className="py-3 pr-4 text-slate-300">{result.phase}</td>
                      <td className="py-3 pr-4 text-slate-300">{result.timeMs.toFixed(2)}</td>
                      <td className="py-3 pr-4 text-slate-300">{result.expansions}</td>
                      <td className="py-3 pr-4 text-slate-300">
                        {Number.isFinite(result.distance) ? result.distance.toFixed(2) : '∞'}
                      </td>
                      <td className="py-3 pr-4 text-slate-300">
                        {result.speedup ? result.speedup.toFixed(2) + '×' : '—'}
                      </td>
                      <td className="py-3 pr-4 text-slate-300">
                        {result.optimality ? result.optimality.toFixed(1) + '%' : '—'}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {auditResults.length > 0 && (
          <div className="bg-surface/70 border border-amber-400/20 rounded-2xl p-6">
            <div className="flex flex-col md:flex-row md:items-center md:justify-between gap-4 mb-4">
              <div>
                <h3 className="text-lg font-semibold">Audit Results (Issue Exposure)</h3>
                <p className="text-sm text-slate-400">
                  Stress cases across random, diameter, and hub/peripheral pairs.
                </p>
              </div>
              <div className="flex items-center gap-3">
                <div className="text-xs uppercase tracking-[0.2em] text-slate-500">
                  Median metrics • Success rate • Optimality
                </div>
                <button
                  onClick={handleCopyAudit}
                  className="text-xs uppercase tracking-[0.2em] px-3 py-2 rounded-lg border border-amber-400/40 text-amber-200 hover:bg-amber-400/10 transition"
                >
                  {auditCopied ? 'Copied' : 'Copy Table'}
                </button>
              </div>
            </div>

            <div className="overflow-x-auto">
              <table className="w-full text-sm text-left">
                <thead className="text-slate-400 border-b border-white/10">
                  <tr>
                    <th className="py-2 pr-4 font-medium">Category</th>
                    <th className="py-2 pr-4 font-medium">Algorithm</th>
                    <th className="py-2 pr-4 font-medium">Phase</th>
                    <th className="py-2 pr-4 font-medium">Median Time</th>
                    <th className="py-2 pr-4 font-medium">Median Exp</th>
                    <th className="py-2 pr-4 font-medium">Success</th>
                    <th className="py-2 pr-4 font-medium">Optimality</th>
                  </tr>
                </thead>
                <tbody>
                  {auditResults.map((result, index) => {
                    const warn = result.successRate < 100 || (result.optimalityMedian !== null && result.optimalityMedian < 100);
                    return (
                      <tr key={`${result.category}-${result.algorithm}-${index}`} className="border-b border-white/5 last:border-0">
                        <td className="py-3 pr-4 text-slate-200">{result.category}</td>
                        <td className={`py-3 pr-4 font-semibold ${warn ? 'text-amber-200' : 'text-white'}`}>
                          {result.algorithm}
                        </td>
                        <td className="py-3 pr-4 text-slate-300">{result.phase}</td>
                        <td className="py-3 pr-4 text-slate-300">{result.medianTime.toFixed(2)} ms</td>
                        <td className="py-3 pr-4 text-slate-300">{result.medianExpansions.toFixed(0)}</td>
                        <td className="py-3 pr-4 text-slate-300">{result.successRate.toFixed(0)}%</td>
                        <td className="py-3 pr-4 text-slate-300">
                          {result.optimalityMedian !== null ? result.optimalityMedian.toFixed(1) + '%' : '—'}
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
          </div>
        )}
      </div>
    </section>
  );
};
