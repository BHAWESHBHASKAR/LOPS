"""
Training pipeline for ML-based node selection.

This script generates training data by simulating actual node contraction on diverse
graph types, trains an MLP model to predict contraction benefit (edge difference),
and saves the model for use in MLNodeSelector.

The training uses edge difference (original_edges - shortcuts) as the label, which is
the standard Contraction Hierarchy heuristic for node importance. This allows the
model to learn actual contraction patterns, not just degree-based heuristics.
"""

import copy
import json
import os
import random
import sys

import numpy as np

sys.path.append(os.path.dirname(os.path.dirname(__file__)))

from algorithms.graph_gen import GraphGenerator
from algorithms.ml_features import extract_node_features, FEATURE_KEYS


def train_mlp(
    x: np.ndarray,
    y: np.ndarray,
    hidden: int = 16,
    epochs: int = 10,
    lr: float = 0.005,
) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """
    Train a simple MLP regressor using stochastic gradient descent.

    This follows the pattern from learned_ch_train.py for consistency.

    Args:
        x: Normalized feature matrix (n_samples, n_features)
        y: Normalized target values (n_samples, 1)
        hidden: Number of hidden units
        epochs: Number of training epochs
        lr: Learning rate

    Returns:
        Tuple of (w1, b1, w2, b2) trained weights
    """
    n, d = x.shape
    rng = np.random.default_rng(42)

    # Initialize weights with small random values
    w1 = rng.normal(scale=0.1, size=(d, hidden)).astype(np.float32)
    b1 = np.zeros((hidden,), dtype=np.float32)
    w2 = rng.normal(scale=0.1, size=(hidden, 1)).astype(np.float32)
    b2 = np.zeros((1,), dtype=np.float32)

    # Training loop with stochastic gradient descent
    for epoch in range(epochs):
        # Shuffle training data
        idx = rng.permutation(n)

        for i in idx:
            xi = x[i : i + 1]
            yi = y[i : i + 1]

            # Forward pass
            h = np.maximum(0.0, xi @ w1 + b1)  # ReLU activation
            pred = h @ w2 + b2
            err = pred - yi

            # Backward pass
            grad_w2 = h.T @ err
            grad_b2 = err[0]
            dh = err @ w2.T
            dh[h <= 0] = 0.0  # ReLU derivative
            grad_w1 = xi.T @ dh
            grad_b1 = dh[0]

            # Gradient clipping for stability
            grad_w2 = np.clip(grad_w2, -1.0, 1.0)
            grad_b2 = np.clip(grad_b2, -1.0, 1.0)
            grad_w1 = np.clip(grad_w1, -1.0, 1.0)
            grad_b1 = np.clip(grad_b1, -1.0, 1.0)

            # Update weights
            w2 -= lr * grad_w2
            b2 -= lr * grad_b2
            w1 -= lr * grad_w1
            b1 -= lr * grad_b1

    return w1, b1, w2, b2


def simulate_contraction_benefit(graph, node: int) -> float:
    """
    Compute edge difference metric for a node (contraction benefit).

    Edge difference is the standard CH heuristic for node importance:
    - Lower edge difference = lower contraction cost = higher priority
    - Edge difference = original_edges - shortcuts_added
    - Negative values mean node contraction adds many shortcuts (high cost)

    Args:
        graph: Graph object
        node: Node ID to evaluate

    Returns:
        Edge difference value (lower is better for contraction)
    """
    # Get node neighbors
    neighbors = graph.edges.get(node, [])
    original_edges = len(neighbors)

    # Simulate shortcuts: for each pair of neighbors, check if direct edge exists
    shortcuts = 0
    neighbor_list = [n for n, _ in neighbors]

    for i in range(len(neighbor_list)):
        for j in range(i + 1, len(neighbor_list)):
            u = neighbor_list[i]
            v = neighbor_list[j]

            # Check if edge exists between u and v
            # If not, contracting 'node' would require adding a shortcut
            u_edges = graph.edges.get(u, [])
            if not any(target == v for target, _ in u_edges):
                shortcuts += 1

    # Edge difference: original edges removed minus shortcuts added
    # Negative means more shortcuts than original edges (high contraction cost)
    edge_difference = original_edges - shortcuts

    return float(edge_difference)


def generate_training_data(
    samples_per_graph: int = 150,
    num_graphs_per_type: int = 3,
) -> tuple[np.ndarray, np.ndarray]:
    """
    Generate training data from multiple graph types.

    For each graph type, we sample nodes and compute:
    1. Features using extract_node_features()
    2. Label (edge difference) using simulate_contraction_benefit()

    This creates diverse training data that captures different graph structures.

    Args:
        samples_per_graph: Number of nodes to sample per graph
        num_graphs_per_type: Number of graphs to generate per type

    Returns:
        Tuple of (X, y) where X is feature matrix and y is labels
    """
    samples = []
    labels = []

    # Graph types to sample from
    graph_configs = [
        # Grid graphs (regular structure)
        {
            "type": "grid",
            "generator": lambda seed: GraphGenerator.grid_graph(15, 15, weighted=False, seed=seed),
            "has_positions": False,
        },
        # Scale-free graphs (hub-and-spoke structure)
        {
            "type": "scale_free",
            "generator": lambda seed: GraphGenerator.scale_free_graph(200, edges_per_node=2, weighted=False, seed=seed),
            "has_positions": False,
        },
        # Geometric graphs (spatial structure)
        {
            "type": "geometric",
            "generator": lambda seed: GraphGenerator.geometric_graph(200, radius=15.0, weighted=False, seed=seed),
            "has_positions": True,
        },
        # Road-like graphs (sparse, clustered)
        {
            "type": "road_like",
            "generator": lambda seed: GraphGenerator.road_network_like(200, avg_degree=4, seed=seed),
            "has_positions": False,
        },
    ]

    for config in graph_configs:
        print(f"Generating training data for {config['type']} graphs...")

        for graph_idx in range(num_graphs_per_type):
            seed = 42 + graph_idx

            if config["has_positions"]:
                graph, positions = config["generator"](seed)
            else:
                graph = config["generator"](seed)
                positions = None

            # Sample nodes randomly
            nodes_to_sample = min(samples_per_graph, graph.nodes)
            sampled_nodes = random.sample(range(graph.nodes), nodes_to_sample)

            for node in sampled_nodes:
                try:
                    # Extract features
                    features = extract_node_features(graph, node, positions)

                    # Compute contraction benefit (edge difference)
                    label = simulate_contraction_benefit(graph, node)

                    # Check for invalid values
                    if not all(np.isfinite(features.get(k, 0.0)) for k in FEATURE_KEYS):
                        continue

                    if not np.isfinite(label):
                        continue

                    # Create feature vector
                    feature_vector = [features[k] for k in FEATURE_KEYS]
                    samples.append(feature_vector)
                    labels.append(label)

                except ValueError:
                    # Skip invalid nodes
                    continue

    print(f"Generated {len(samples)} total samples")

    X = np.array(samples, dtype=np.float32)
    y = np.array(labels, dtype=np.float32).reshape(-1, 1)

    return X, y


def main():
    """Main training pipeline."""
    # Set random seeds for reproducibility
    random.seed(42)
    np.random.seed(42)

    print("Starting ML node selector training...")
    print("=" * 60)

    # Generate training data
    print("\n1. Generating training data...")
    X, y = generate_training_data(samples_per_graph=150, num_graphs_per_type=3)

    # Filter infinite values
    print("\n2. Filtering invalid samples...")
    finite_mask = np.isfinite(X).all(axis=1) & np.isfinite(y).reshape(-1)
    X = X[finite_mask]
    y = y[finite_mask]
    print(f"   Valid samples: {len(X)}")

    # Normalize features
    print("\n3. Normalizing features...")
    mean = X.mean(axis=0)
    std = X.std(axis=0) + 1e-6
    X_norm = (X - mean) / std

    # Normalize labels
    y_mean = float(y.mean())
    y_std = float(y.std() + 1e-6)
    y_norm = (y - y_mean) / y_std

    print(f"   Features: {X.shape[1]}")
    print(f"   Samples: {X.shape[0]}")
    print(f"   Label mean: {y_mean:.4f}")
    print(f"   Label std: {y_std:.4f}")

    # Train model
    print("\n4. Training MLP model...")
    hidden_size = 16
    epochs = 10
    learning_rate = 0.005

    w1, b1, w2, b2 = train_mlp(X_norm, y_norm, hidden=hidden_size, epochs=epochs, lr=learning_rate)

    # Compute final MSE
    h = np.maximum(0.0, X_norm @ w1 + b1)
    pred = h @ w2 + b2
    mse = float(np.mean((pred - y_norm) ** 2))
    r2 = float(1 - np.sum((y_norm - pred) ** 2) / np.sum((y_norm - y_norm.mean()) ** 2))

    print(f"   Hidden units: {hidden_size}")
    print(f"   Epochs: {epochs}")
    print(f"   Final MSE (normalized): {mse:.6f}")
    print(f"   R² score: {r2:.4f}")

    # Create model dict
    print("\n5. Saving model...")
    model = {
        "feature_keys": FEATURE_KEYS,
        "mean": mean.tolist(),
        "std": std.tolist(),
        "y_mean": y_mean,
        "y_std": y_std,
        "w1": w1.tolist(),
        "b1": b1.tolist(),
        "w2": w2.tolist(),
        "b2": b2.tolist(),
    }

    # Create models directory if needed
    os.makedirs("models", exist_ok=True)

    # Save model
    model_path = "models/ml_node_selector.json"
    with open(model_path, "w") as f:
        json.dump(model, f, indent=2)

    print(f"   Model saved to: {model_path}")

    # Print summary
    print("\n" + "=" * 60)
    print("Training complete!")
    print(f"Model: {model_path}")
    print(f"Samples: {X.shape[0]}")
    print(f"Features: {len(FEATURE_KEYS)}")
    print(f"MSE: {mse:.6f}")
    print(f"R²: {r2:.4f}")
    print("=" * 60)


if __name__ == "__main__":
    main()
