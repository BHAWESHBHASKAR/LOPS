import csv
import os
from collections import defaultdict

INPUT_CSV = "/Users/bhaweshbhaskar/Desktop/isitpossible/research/lips_budget_ablation.csv"
OUT_DIR = "/Users/bhaweshbhaskar/Desktop/isitpossible/research/figures"

os.makedirs(OUT_DIR, exist_ok=True)

# Data structure: graph -> budget -> variant -> metrics

data = defaultdict(lambda: defaultdict(dict))

with open(INPUT_CSV, "r", newline="") as f:
    reader = csv.DictReader(f)
    for row in reader:
        graph = row["graph"]
        budget = float(row["budget_ms"])
        variant = row["variant"]
        mean = float(row["mean"])
        expansions = float(row["avg_expansions"])
        data[graph][budget][variant] = {
            "mean": mean,
            "expansions": expansions,
        }

variants = ["ALT-only", "LIPS-only", "Combined"]
colors = {
    "ALT-only": "#1f77b4",
    "LIPS-only": "#ff7f0e",
    "Combined": "#2ca02c",
}
markers = {
    "ALT-only": "circle",
    "LIPS-only": "square",
    "Combined": "triangle",
}


def svg_escape(text: str) -> str:
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def format_tick(value: float) -> str:
    if abs(value) >= 1000:
        return f"{value:.0f}"
    if abs(value) >= 100:
        return f"{value:.0f}"
    if abs(value) >= 10:
        return f"{value:.1f}"
    return f"{value:.2f}"


def write_svg_line_chart(graph, metric, ylabel, filename_suffix):
    budgets = sorted(data[graph].keys())

    # Collect y values
    y_values = []
    for v in variants:
        for b in budgets:
            y_values.append(data[graph][b][v][metric])

    y_min = min(y_values)
    y_max = max(y_values)
    if y_min == y_max:
        y_min -= 1.0
        y_max += 1.0
    pad = (y_max - y_min) * 0.08
    y_min -= pad
    y_max += pad

    # Layout
    width = 800
    height = 500
    margin_left = 70
    margin_right = 30
    margin_top = 50
    margin_bottom = 60

    plot_width = width - margin_left - margin_right
    plot_height = height - margin_top - margin_bottom

    def x_map(budget):
        return margin_left + (budget - budgets[0]) / (budgets[-1] - budgets[0]) * plot_width

    def y_map(val):
        return margin_top + (y_max - val) / (y_max - y_min) * plot_height

    # Axes ticks
    x_ticks = budgets
    y_ticks = 5
    y_tick_values = [y_min + i * (y_max - y_min) / (y_ticks - 1) for i in range(y_ticks)]

    lines = []
    lines.append(f"<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}'>")
    lines.append("<rect width='100%' height='100%' fill='white' />")

    # Title
    lines.append(f"<text x='{width/2}' y='{margin_top - 20}' text-anchor='middle' font-size='16' font-family='Arial'>{svg_escape(graph)}</text>")

    # Axes
    x0 = margin_left
    y0 = margin_top + plot_height
    x1 = margin_left + plot_width
    y1 = margin_top
    lines.append(f"<line x1='{x0}' y1='{y0}' x2='{x1}' y2='{y0}' stroke='#333' stroke-width='1' />")
    lines.append(f"<line x1='{x0}' y1='{y0}' x2='{x0}' y2='{y1}' stroke='#333' stroke-width='1' />")

    # X ticks
    for b in x_ticks:
        x = x_map(b)
        lines.append(f"<line x1='{x}' y1='{y0}' x2='{x}' y2='{y0 + 6}' stroke='#333' stroke-width='1' />")
        lines.append(f"<text x='{x}' y='{y0 + 22}' text-anchor='middle' font-size='12' font-family='Arial'>{format_tick(b)}</text>")

    # Y ticks
    for v in y_tick_values:
        y = y_map(v)
        lines.append(f"<line x1='{x0 - 6}' y1='{y}' x2='{x0}' y2='{y}' stroke='#333' stroke-width='1' />")
        lines.append(f"<text x='{x0 - 10}' y='{y + 4}' text-anchor='end' font-size='12' font-family='Arial'>{format_tick(v)}</text>")
        lines.append(f"<line x1='{x0}' y1='{y}' x2='{x1}' y2='{y}' stroke='#eee' stroke-width='1' />")

    # Axis labels
    lines.append(f"<text x='{width/2}' y='{height - 20}' text-anchor='middle' font-size='13' font-family='Arial'>Preprocessing budget (ms)</text>")
    lines.append(f"<text x='20' y='{height/2}' text-anchor='middle' font-size='13' font-family='Arial' transform='rotate(-90 20 {height/2})'>{svg_escape(ylabel)}</text>")

    # Plot lines
    for v in variants:
        points = [(x_map(b), y_map(data[graph][b][v][metric])) for b in budgets]
        path = " ".join([f"{x},{y}" for x, y in points])
        lines.append(f"<polyline fill='none' stroke='{colors[v]}' stroke-width='2' points='{path}' />")
        for x, y in points:
            if markers[v] == "circle":
                lines.append(f"<circle cx='{x}' cy='{y}' r='4' fill='{colors[v]}' />")
            elif markers[v] == "square":
                lines.append(f"<rect x='{x-4}' y='{y-4}' width='8' height='8' fill='{colors[v]}' />")
            else:
                lines.append(f"<polygon points='{x},{y-5} {x-5},{y+4} {x+5},{y+4}' fill='{colors[v]}' />")

    # Legend
    legend_x = x1 - 140
    legend_y = y1 + 10
    lines.append(f"<rect x='{legend_x - 10}' y='{legend_y - 18}' width='150' height='60' fill='white' stroke='#ddd' />")
    for i, v in enumerate(variants):
        y = legend_y + i * 18
        lines.append(f"<rect x='{legend_x}' y='{y - 8}' width='10' height='10' fill='{colors[v]}' />")
        lines.append(f"<text x='{legend_x + 16}' y='{y}' font-size='12' font-family='Arial'>{v}</text>")

    lines.append("</svg>")

    out_path = os.path.join(
        OUT_DIR,
        f"{graph.replace(' ', '_').replace('(', '').replace(')', '').replace('/', '_')}_{filename_suffix}.svg",
    )

    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    return out_path


outputs = []
for graph in sorted(data.keys()):
    outputs.append(write_svg_line_chart(graph, "mean", "Mean tightness (h/d*)", "tightness"))
    outputs.append(write_svg_line_chart(graph, "expansions", "Average A* expansions", "expansions"))

print("Generated plots:")
for p in outputs:
    print(p)
