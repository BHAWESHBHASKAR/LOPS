/**
 * LIPS Budget-Matched Ablation
 * Matches preprocessing wall-time between ALT-only, LIPS-only, and Combined.
 */

#include "../include/photon/lops.hpp"
#include "../include/photon/search.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace photon;

namespace {

struct TightnessStats {
    double mean = 0.0;
    double median = 0.0;
    double p90 = 0.0;
    double p99 = 0.0;
    double min = 0.0;
    double max = 0.0;
    size_t count = 0;
};

struct VariantEval {
    std::string name;
    lops::LOPSParams params;
    double preprocessing_us = 0.0;
    TightnessStats tightness;
    double avg_expansions = 0.0;
    int optimal_count = 0;
    int total_queries = 0;
};

struct BudgetMatch {
    double budget_ms = 0.0;
    VariantEval alt;
    VariantEval lips;
    VariantEval combined;
};

struct QuerySet {
    std::vector<std::pair<NodeId, NodeId>> queries;
    std::vector<float> optimal;
};

TightnessStats summarize(std::vector<double> ratios) {
    TightnessStats stats;
    if (ratios.empty()) return stats;

    std::sort(ratios.begin(), ratios.end());
    stats.count = ratios.size();
    stats.min = ratios.front();
    stats.max = ratios.back();

    double sum = 0.0;
    for (double r : ratios) sum += r;
    stats.mean = sum / static_cast<double>(ratios.size());

    auto percentile = [&](double p) {
        double idx = p * (ratios.size() - 1);
        size_t i = static_cast<size_t>(idx);
        double frac = idx - static_cast<double>(i);
        if (i + 1 < ratios.size()) {
            return ratios[i] * (1.0 - frac) + ratios[i + 1] * frac;
        }
        return ratios[i];
    };

    stats.median = percentile(0.50);
    stats.p90 = percentile(0.90);
    stats.p99 = percentile(0.99);
    return stats;
}

std::vector<std::pair<NodeId, NodeId>> make_queries(const Graph& graph, size_t count, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<NodeId> dist(0, static_cast<NodeId>(graph.num_nodes() - 1));
    std::vector<std::pair<NodeId, NodeId>> queries;
    queries.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        NodeId s = dist(rng);
        NodeId t = dist(rng);
        while (t == s) t = dist(rng);
        queries.push_back({s, t});
    }
    return queries;
}

std::vector<float> compute_optimal(const Graph& graph, const std::vector<std::pair<NodeId, NodeId>>& queries) {
    std::vector<float> dist;
    dist.reserve(queries.size());
    for (auto [s, t] : queries) {
        auto r = search::dijkstra(graph, s, t);
        dist.push_back(r.distance);
    }
    return dist;
}

struct AStarResult {
    float distance = INF_DIST;
    size_t nodes_explored = 0;
};

AStarResult a_star_with_heuristic(
    const Graph& graph,
    NodeId source,
    NodeId target,
    const std::function<float(NodeId)>& heuristic
) {
    AStarResult result;
    const size_t n = graph.num_nodes();
    if (source >= n || target >= n) return result;
    if (source == target) {
        result.distance = 0.0f;
        return result;
    }

    std::vector<float> g_score(n, INF_DIST);
    std::vector<bool> closed(n, false);

    struct PQItem { float f; float g; NodeId node; };
    struct PQCmp { bool operator()(const PQItem& a, const PQItem& b) const { return a.f > b.f; } };
    std::priority_queue<PQItem, std::vector<PQItem>, PQCmp> pq;

    g_score[source] = 0.0f;
    pq.push({heuristic(source), 0.0f, source});

    while (!pq.empty()) {
        auto [f, g, u] = pq.top();
        pq.pop();

        if (g != g_score[u]) continue;
        if (closed[u]) continue;
        closed[u] = true;
        ++result.nodes_explored;

        if (u == target) {
            result.distance = g_score[u];
            return result;
        }

        for (const auto& e : graph.out_edges(u)) {
            float ng = g_score[u] + e.weight;
            if (ng < g_score[e.target]) {
                g_score[e.target] = ng;
                pq.push({ng + heuristic(e.target), ng, e.target});
            }
        }
    }

    return result;
}

lops::LOPSParams base_params() {
    lops::LOPSParams p;
    p.use_bidirectional = false;
    p.bidir_for_exact = false;
    p.bidir_for_weighted = false;
    p.symmetric_labels = true;
    p.degree_weighted = true;
    p.seed = 42;
    return p;
}

double measure_preprocess_us(const Graph& graph, const lops::LOPSParams& params) {
    lops::LOPS solver;
    solver.preprocess(graph, params);
    return solver.get_preprocessing_time_us();
}

lops::LOPSParams params_for_alt(uint32_t landmarks) {
    auto p = base_params();
    p.use_landmarks = true;
    p.num_landmarks = landmarks;
    p.num_potentials = 0;
    p.num_anchors = 0;
    return p;
}

lops::LOPSParams params_for_lips(uint32_t potentials) {
    auto p = base_params();
    p.use_landmarks = false;
    p.num_potentials = potentials;
    p.num_anchors = 0;
    return p;
}

lops::LOPSParams params_for_combined(uint32_t count) {
    auto p = base_params();
    p.use_landmarks = true;
    p.num_landmarks = count;
    p.num_potentials = count;
    p.num_anchors = 0;
    return p;
}

struct TuneResult {
    lops::LOPSParams params;
    double time_us = 0.0;
};

TuneResult tune_single_knob(
    const Graph& graph,
    const std::function<lops::LOPSParams(uint32_t)>& make_params,
    double target_us
) {
    const uint32_t min_count = 2;
    const uint32_t max_count = 4096;

    uint32_t count = min_count;
    double time_us = measure_preprocess_us(graph, make_params(count));
    uint32_t prev = count;
    double prev_time = time_us;

    while (time_us < target_us * 0.95 && count < max_count) {
        prev = count;
        prev_time = time_us;
        count = std::min<uint32_t>(count * 2, max_count);
        time_us = measure_preprocess_us(graph, make_params(count));
    }

    uint32_t best_count = (std::abs(time_us - target_us) < std::abs(prev_time - target_us)) ? count : prev;
    double best_time = (std::abs(time_us - target_us) < std::abs(prev_time - target_us)) ? time_us : prev_time;

    uint32_t low = (count == min_count) ? min_count : prev;
    uint32_t high = count;
    for (int i = 0; i < 8 && low <= high; ++i) {
        uint32_t mid = low + (high - low) / 2;
        double mid_time = measure_preprocess_us(graph, make_params(mid));
        if (std::abs(mid_time - target_us) < std::abs(best_time - target_us)) {
            best_time = mid_time;
            best_count = mid;
        }
        if (mid_time < target_us) {
            low = mid + 1;
        } else {
            if (mid == 0) break;
            high = mid - 1;
        }
    }

    TuneResult result;
    result.params = make_params(best_count);
    result.time_us = best_time;
    return result;
}

VariantEval evaluate_variant(
    const Graph& graph,
    const lops::LOPSParams& params,
    const std::string& name,
    const QuerySet& query_set,
    const std::function<float(const lops::LOPS&, NodeId, NodeId)>& heuristic_fn
) {
    lops::LOPS solver;
    solver.preprocess(graph, params);

    std::vector<double> ratios;
    ratios.reserve(query_set.queries.size());

    double total_expansions = 0.0;
    int optimal = 0;

    for (size_t i = 0; i < query_set.queries.size(); ++i) {
        auto [s, t] = query_set.queries[i];
        float d = query_set.optimal[i];
        if (d >= INF_DIST || d <= 0.0f) continue;

        float h = heuristic_fn(solver, s, t);
        ratios.push_back(static_cast<double>(h) / static_cast<double>(d));

        auto res = a_star_with_heuristic(graph, s, t, [&](NodeId v) {
            return heuristic_fn(solver, v, t);
        });
        total_expansions += static_cast<double>(res.nodes_explored);
        if (res.distance <= d * 1.001f) optimal++;
    }

    VariantEval out;
    out.name = name;
    out.params = params;
    out.preprocessing_us = solver.get_preprocessing_time_us();
    out.tightness = summarize(std::move(ratios));
    out.avg_expansions = total_expansions / static_cast<double>(query_set.queries.size());
    out.optimal_count = optimal;
    out.total_queries = static_cast<int>(query_set.queries.size());
    return out;
}

BudgetMatch run_budget_match(
    const Graph& graph,
    double budget_ms,
    const QuerySet& query_set
) {
    double target_us = budget_ms * 1000.0;

    auto alt_tuned = tune_single_knob(graph, params_for_alt, target_us);
    auto lips_tuned = tune_single_knob(graph, params_for_lips, target_us);

    // Align closer if mismatch is large.
    double diff = std::abs(alt_tuned.time_us - lips_tuned.time_us);
    double avg = (alt_tuned.time_us + lips_tuned.time_us) * 0.5;
    if (avg > 0.0 && diff / avg > 0.05) {
        double new_target = std::max(alt_tuned.time_us, lips_tuned.time_us);
        alt_tuned = tune_single_knob(graph, params_for_alt, new_target);
        lips_tuned = tune_single_knob(graph, params_for_lips, new_target);
    }

    double combined_target = (alt_tuned.time_us + lips_tuned.time_us) * 0.5;
    auto combined_tuned = tune_single_knob(graph, params_for_combined, combined_target);

    BudgetMatch match;
    match.budget_ms = budget_ms;

    match.alt = evaluate_variant(
        graph,
        alt_tuned.params,
        "ALT-only",
        query_set,
        [](const lops::LOPS& s, NodeId v, NodeId t) { return s.heuristic_alt(v, t); }
    );

    match.lips = evaluate_variant(
        graph,
        lips_tuned.params,
        "LIPS-only",
        query_set,
        [](const lops::LOPS& s, NodeId v, NodeId t) { return s.heuristic_lips(v, t); }
    );

    match.combined = evaluate_variant(
        graph,
        combined_tuned.params,
        "Combined",
        query_set,
        [](const lops::LOPS& s, NodeId v, NodeId t) { return s.heuristic_combined(v, t); }
    );

    return match;
}

void print_budget_table(const std::string& graph_name, const std::vector<BudgetMatch>& matches) {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Budget-matched tightness: " << graph_name << "\n";
    std::cout << "============================================================\n";
    std::cout << std::left << std::setw(16) << "Budget(ms)"
              << std::setw(14) << "ALT"
              << std::setw(14) << "LIPS"
              << std::setw(14) << "Combined" << "\n";
    std::cout << "------------------------------------------------------------\n";

    for (const auto& m : matches) {
        std::cout << std::left << std::setw(16) << std::fixed << std::setprecision(0) << m.budget_ms
                  << std::setw(14) << std::setprecision(3) << m.alt.tightness.mean
                  << std::setw(14) << m.lips.tightness.mean
                  << std::setw(14) << m.combined.tightness.mean << "\n";
    }
}

void print_expansion_table(const std::string& graph_name, const std::vector<BudgetMatch>& matches) {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "Budget-matched expansions: " << graph_name << "\n";
    std::cout << "============================================================\n";
    std::cout << std::left << std::setw(16) << "Budget(ms)"
              << std::setw(14) << "ALT"
              << std::setw(14) << "LIPS"
              << std::setw(14) << "Combined" << "\n";
    std::cout << "------------------------------------------------------------\n";

    for (const auto& m : matches) {
        std::cout << std::left << std::setw(16) << std::fixed << std::setprecision(0) << m.budget_ms
                  << std::setw(14) << std::setprecision(1) << m.alt.avg_expansions
                  << std::setw(14) << m.lips.avg_expansions
                  << std::setw(14) << m.combined.avg_expansions << "\n";
    }
}

void write_csv(std::ofstream& out, const std::string& graph_name, const std::vector<BudgetMatch>& matches) {
    for (const auto& m : matches) {
        auto write_row = [&](const VariantEval& v) {
            out << graph_name << "," << m.budget_ms << "," << v.name << ","
                << v.preprocessing_us << "," << v.tightness.mean << "," << v.tightness.median << ","
                << v.tightness.p90 << "," << v.tightness.p99 << ","
                << v.avg_expansions << "," << v.optimal_count << "," << v.total_queries << "\n";
        };
        write_row(m.alt);
        write_row(m.lips);
        write_row(m.combined);
    }
}

QuerySet build_query_set(const Graph& graph, size_t count) {
    QuerySet set;
    set.queries = make_queries(graph, count, 42);
    set.optimal = compute_optimal(graph, set.queries);
    return set;
}

} // namespace

int main() {
    const std::vector<double> budgets_ms = {50.0, 200.0, 800.0};
    const size_t NUM_QUERIES = 120;

    std::ofstream csv("/Users/bhaweshbhaskar/Desktop/isitpossible/research/lips_budget_ablation.csv");
    if (!csv) {
        std::cerr << "Failed to open output CSV.\n";
        return 1;
    }
    csv << "graph,budget_ms,variant,preprocess_us,mean,median,p90,p99,avg_expansions,optimal_count,total_queries\n";

    struct GraphCase {
        std::string name;
        Graph graph;
    };

    std::vector<GraphCase> graphs;
    graphs.push_back({"Scale-Free 5K (alpha=2.2)", generators::scale_free(5000, 2.2f)});
    graphs.push_back({"Social 5K (k=12)", generators::social_network(5000, 12)});
    graphs.push_back({"Road 5K", generators::road_network(5000, 7)});
    graphs.push_back({"Grid 50x50", generators::grid(50, 50, true, true)});
    graphs.push_back({"Random 5K (p=0.004)", generators::random_graph(5000, 0.004f, true)});

    for (auto& gcase : graphs) {
        auto query_set = build_query_set(gcase.graph, NUM_QUERIES);
        std::vector<BudgetMatch> matches;
        matches.reserve(budgets_ms.size());
        for (double budget : budgets_ms) {
            matches.push_back(run_budget_match(gcase.graph, budget, query_set));
        }

        print_budget_table(gcase.name, matches);
        print_expansion_table(gcase.name, matches);
        write_csv(csv, gcase.name, matches);
    }

    std::cout << "\nResults exported to /Users/bhaweshbhaskar/Desktop/isitpossible/research/lips_budget_ablation.csv\n";
    return 0;
}
