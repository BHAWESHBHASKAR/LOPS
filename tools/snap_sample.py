import argparse
import json
import math
import random
from collections import deque, defaultdict


def read_edges(path: str):
    edges = []
    degrees = defaultdict(int)
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.strip().split()
            if len(parts) < 2:
                continue
            a = int(parts[0])
            b = int(parts[1])
            if a == b:
                continue
            edges.append((a, b))
            degrees[a] += 1
            degrees[b] += 1
    return edges, degrees


def build_adjacency(edges):
    adj = defaultdict(list)
    for a, b in edges:
        adj[a].append(b)
        adj[b].append(a)
    return adj


def bfs_sample(adj, seed, target_size):
    visited = set([seed])
    order = [seed]
    queue = deque([seed])
    while queue and len(visited) < target_size:
        node = queue.popleft()
        for n in adj[node]:
            if n in visited:
                continue
            visited.add(n)
            order.append(n)
            queue.append(n)
            if len(visited) >= target_size:
                break
    return visited, order


def choose_seed(degrees, rng, low=0.2, high=0.6):
    items = sorted(degrees.items(), key=lambda item: item[1])
    if not items:
        return 0
    lo = int(len(items) * low)
    hi = max(lo + 1, int(len(items) * high))
    idx = rng.randint(lo, min(len(items) - 1, hi))
    return items[idx][0]


def bfs_distances(adj, source):
    queue = deque([source])
    dist = {source: 0}
    while queue:
        node = queue.popleft()
        for n in adj[node]:
            if n in dist:
                continue
            dist[n] = dist[node] + 1
            queue.append(n)
    return dist


def farthest_node(adj, source):
    dist = bfs_distances(adj, source)
    farthest, far_dist = max(dist.items(), key=lambda item: item[1])
    return farthest, far_dist


def double_sweep(adj, seed):
    a, _ = farthest_node(adj, seed)
    b, dist = farthest_node(adj, a)
    return a, b, dist


def layout_nodes(node_ids, degrees, width=760, height=360):
    rng = random.Random(42)
    center_x = width / 2
    center_y = height / 2
    max_deg = max(degrees.values()) if degrees else 1
    nodes = []
    for i, node in enumerate(node_ids):
        angle = (2 * math.pi * i) / max(len(node_ids), 1)
        deg = degrees.get(node, 1)
        radius = 50 + (1 - deg / max_deg) * 150 + rng.uniform(-10, 10)
        x = center_x + math.cos(angle) * radius
        y = center_y + math.sin(angle) * radius * 0.75
        nodes.append({"id": node, "x": round(x, 2), "y": round(y, 2), "degree": deg})
    return nodes


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="SNAP edge list .txt file")
    parser.add_argument("output", help="Output JSON path")
    parser.add_argument("--size", type=int, default=220, help="Sample size")
    parser.add_argument("--name", default="SNAP Sample", help="Dataset name")
    args = parser.parse_args()

    edges, degrees = read_edges(args.input)
    if not edges:
        raise SystemExit("No edges found in input file.")

    rng = random.Random(42)
    adj = build_adjacency(edges)
    sample_nodes = set()
    order = []
    for _ in range(12):
        seed = choose_seed(degrees, rng)
        sample_nodes, order = bfs_sample(adj, seed, args.size)
        if len(sample_nodes) >= args.size:
            break

    sample_edges = []
    for a, b in edges:
        if a in sample_nodes and b in sample_nodes:
            sample_edges.append((a, b))

    mapping = {node: idx for idx, node in enumerate(order)}
    mapped_edges = [{"s": mapping[a], "t": mapping[b]} for a, b in sample_edges]

    sample_degrees = defaultdict(int)
    for e in mapped_edges:
        sample_degrees[e["s"]] += 1
        sample_degrees[e["t"]] += 1

    nodes = layout_nodes(list(range(len(mapping))), sample_degrees)

    adj_sample = build_adjacency([(e["s"], e["t"]) for e in mapped_edges])
    node_count = len(mapping)
    if node_count == 0:
        source, target = 0, 0
    else:
        candidate_nodes = list(range(node_count))
        rng.shuffle(candidate_nodes)
        source = candidate_nodes[0]
        target = candidate_nodes[0]
        best_dist = -1
        for seed in candidate_nodes[:12]:
            a, b, dist = double_sweep(adj_sample, seed)
            if dist > best_dist:
                source, target, best_dist = a, b, dist
            if best_dist >= 3:
                break

    payload = {
        "name": args.name,
        "nodeCount": len(nodes),
        "edgeCount": len(mapped_edges),
        "source": source,
        "target": target,
        "nodes": nodes,
        "edges": mapped_edges,
    }

    with open(args.output, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)


if __name__ == "__main__":
    main()
