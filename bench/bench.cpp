// Permanent search benchmark suite.
//
// Runs every registered Search implementation over every position in
// bench/positions.cpp at a fixed depth and reports, per (position,
// algorithm) pair and in aggregate:
//   - nodes searched
//   - nodes per second (NPS)
//   - cutoffs (beta cutoffs, null-move cutoffs, TT cutoffs)
//   - transposition table hit rate
//   - move ordering quality (% of beta cutoffs on the first move tried)
//   - effective branching factor (EBF) across the iterative-deepening depths
//
// Usage:
//   bench_search [--depth N] [--max-nodes N] [--time-ms N] [--csv PATH]

#include "eval/bughouse.h"
#include "evaluator.h"
#include "game/board.h"
#include "positions.h"
#include "search/alpha_beta_search.h"
#include "search/null_move_search.h"
#include "search/pvs.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Args {
  int depth = 5;
  uint64_t max_nodes = 5'000'000; // safety net so a bad position can't hang
  int time_ms = 15'000;           // per-search wall-clock safety net
  std::string csv_path = "bench/results/latest.csv";
};

Args parse_args(int argc, char **argv) {
  Args args;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc)
        throw std::runtime_error("bench: missing value for " + arg);
      return argv[++i];
    };
    if (arg == "--depth")
      args.depth = std::stoi(next());
    else if (arg == "--max-nodes")
      args.max_nodes = std::stoull(next());
    else if (arg == "--time-ms")
      args.time_ms = std::stoi(next());
    else if (arg == "--csv")
      args.csv_path = next();
    else
      throw std::runtime_error("bench: unknown argument '" + arg + "'");
  }
  return args;
}

struct BenchRow {
  std::string position;
  std::string algorithm;

  uint64_t nodes = 0;
  double nps = 0.0;
  int depth_reached = 0;
  int64_t elapsed_ms = 0;

  uint64_t beta_cutoffs = 0;
  uint64_t null_move_cutoffs = 0;
  uint64_t tt_cutoffs = 0;

  uint64_t tt_probes = 0;
  uint64_t tt_hits = 0;
  double tt_hit_rate = 0.0; // tt_hits / tt_probes

  uint64_t first_move_cutoffs = 0;
  double move_ordering_quality = 0.0; // first_move_cutoffs / beta_cutoffs

  double effective_branching_factor = 0.0;

  std::string best_move;
  int score = 0;
};

double compute_ebf(const std::vector<uint64_t> &nodes_by_depth) {
  // nodes_by_depth[i] is the cumulative node count through depth i+1.
  // For a uniform tree, N(d) ~= b^d, so b = (N(d)/N(1))^(1/(d-1)).
  size_t d = nodes_by_depth.size();
  if (d < 2 || nodes_by_depth.front() == 0)
    return 0.0;

  double n1 = static_cast<double>(nodes_by_depth.front());
  double nd = static_cast<double>(nodes_by_depth.back());
  if (nd <= n1)
    return 0.0;

  return std::pow(nd / n1, 1.0 / static_cast<double>(d - 1));
}

BenchRow run_one(const std::string &algorithm, Search &search,
                 const BenchPosition &spec, const BughousePosition &position,
                 const Args &args) {
  BughouseClock clock;
  clock.set(5 * 60 * 1000, 2000);

  CommunicationContext comm_context{};
  SearchContext context =
      make_context(clock, to_player(spec.root_player), comm_context);
  SearchLimits limits;
  limits.max_depth = args.depth;
  limits.max_nodes = args.max_nodes;
  limits.move_time = std::chrono::milliseconds(args.time_ms);

  std::stop_source stop;
  SearchResult result =
      search.search(position, context, limits, stop.get_token());

  BenchRow row;
  row.position = spec.name;
  row.algorithm = algorithm;
  row.nodes = result.stats.nodes;
  row.depth_reached = result.stats.depth_reached;
  row.elapsed_ms = result.stats.elapsed.count();
  row.nps = row.elapsed_ms > 0
                ? static_cast<double>(row.nodes) * 1000.0 / row.elapsed_ms
                : static_cast<double>(row.nodes); // finished in <1ms
  row.beta_cutoffs = result.stats.beta_cutoffs;
  row.null_move_cutoffs = result.stats.null_move_cutoffs;
  row.tt_cutoffs = result.stats.tt_stats.cutoffs;
  row.tt_probes = result.stats.tt_stats.probes;
  row.tt_hits = result.stats.tt_stats.hits;
  row.tt_hit_rate = row.tt_probes > 0
                        ? static_cast<double>(row.tt_hits) / row.tt_probes
                        : 0.0;
  row.first_move_cutoffs = result.stats.first_move_cutoffs;
  row.move_ordering_quality =
      row.beta_cutoffs > 0
          ? static_cast<double>(row.first_move_cutoffs) / row.beta_cutoffs
          : 0.0;
  row.effective_branching_factor = compute_ebf(result.stats.nodes_by_depth);
  row.best_move =
      result.best_move.is_none() ? "(none)" : result.best_move.to_string();
  row.score = result.score;
  return row;
}

void print_table(const std::vector<BenchRow> &rows) {
  std::printf("%-20s %-11s %10s %12s %5s %8s %6s %6s %7s %7s %6s %7s\n",
              "position", "algorithm", "nodes", "nps", "depth", "time_ms",
              "beta_c", "nm_c", "tt_hit%", "order%", "ebf", "move");
  std::printf("%s\n", std::string(120, '-').c_str());
  for (const BenchRow &r : rows) {
    std::printf("%-20s %-11s %10llu %12.0f %5d %8lld %6llu %6llu %6.1f%% "
                "%6.1f%% %6.2f %7s\n",
                r.position.c_str(), r.algorithm.c_str(),
                static_cast<unsigned long long>(r.nodes), r.nps,
                r.depth_reached, static_cast<long long>(r.elapsed_ms),
                static_cast<unsigned long long>(r.beta_cutoffs),
                static_cast<unsigned long long>(r.null_move_cutoffs),
                r.tt_hit_rate * 100.0, r.move_ordering_quality * 100.0,
                r.effective_branching_factor, r.best_move.c_str());
  }
}

// Aggregate rows for one algorithm across the whole suite: totals for
// counters, weighted rates, and a geometric mean for EBF.
BenchRow aggregate(const std::string &algorithm,
                   const std::vector<BenchRow> &rows) {
  BenchRow agg;
  agg.position = "ALL";
  agg.algorithm = algorithm;

  double ebf_log_sum = 0.0;
  int ebf_count = 0;
  int64_t total_ms = 0;

  for (const BenchRow &r : rows) {
    if (r.algorithm != algorithm)
      continue;
    agg.nodes += r.nodes;
    agg.beta_cutoffs += r.beta_cutoffs;
    agg.null_move_cutoffs += r.null_move_cutoffs;
    agg.tt_cutoffs += r.tt_cutoffs;
    agg.tt_probes += r.tt_probes;
    agg.tt_hits += r.tt_hits;
    agg.first_move_cutoffs += r.first_move_cutoffs;
    total_ms += r.elapsed_ms;
    if (r.effective_branching_factor > 0.0) {
      ebf_log_sum += std::log(r.effective_branching_factor);
      ebf_count++;
    }
  }

  agg.elapsed_ms = total_ms;
  agg.nps = total_ms > 0 ? static_cast<double>(agg.nodes) * 1000.0 / total_ms
                         : static_cast<double>(agg.nodes);
  agg.tt_hit_rate = agg.tt_probes > 0
                        ? static_cast<double>(agg.tt_hits) / agg.tt_probes
                        : 0.0;
  agg.move_ordering_quality =
      agg.beta_cutoffs > 0
          ? static_cast<double>(agg.first_move_cutoffs) / agg.beta_cutoffs
          : 0.0;
  agg.effective_branching_factor =
      ebf_count > 0 ? std::exp(ebf_log_sum / ebf_count) : 0.0;
  agg.best_move = "-";
  return agg;
}

void write_csv(const std::string &path, const std::vector<BenchRow> &rows) {
  std::filesystem::path p(path);
  if (p.has_parent_path())
    std::filesystem::create_directories(p.parent_path());

  std::ofstream out(path);
  out << "position,algorithm,nodes,nps,depth_reached,elapsed_ms,"
         "beta_cutoffs,null_move_cutoffs,tt_cutoffs,tt_probes,tt_hits,"
         "tt_hit_rate,first_move_cutoffs,move_ordering_quality,"
         "effective_branching_factor,score,best_move\n";
  out << std::fixed << std::setprecision(6);
  for (const BenchRow &r : rows) {
    out << r.position << ',' << r.algorithm << ',' << r.nodes << ',' << r.nps
        << ',' << r.depth_reached << ',' << r.elapsed_ms << ','
        << r.beta_cutoffs << ',' << r.null_move_cutoffs << ',' << r.tt_cutoffs
        << ',' << r.tt_probes << ',' << r.tt_hits << ',' << r.tt_hit_rate << ','
        << r.first_move_cutoffs << ',' << r.move_ordering_quality << ','
        << r.effective_branching_factor << ',' << r.score << ',' << r.best_move
        << '\n';
  }
}

std::string timestamped_path(const std::string &base) {
  std::filesystem::path p(base);
  std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_r(&now, &tm);
  std::ostringstream stamp;
  stamp << std::put_time(&tm, "%Y%m%d-%H%M%S");

  std::filesystem::path stamped =
      p.parent_path() /
      (p.stem().string() + "-" + stamp.str() + p.extension().string());
  return stamped.string();
}

} // namespace

int main(int argc, char **argv) {
  Args args;
  try {
    args = parse_args(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }

  Board::init_zobrist();

  BughouseEvaluator evaluator;
  const std::vector<BenchPosition> &suite = benchmark_suite();

  std::vector<BenchRow> rows;

  for (const BenchPosition &spec : suite) {
    BughousePosition position;
    try {
      position = build_position(spec);
    } catch (const std::exception &e) {
      std::cerr << "Skipping '" << spec.name << "': " << e.what() << '\n';
      continue;
    }

    SearchParams params;

    TranspositionTable tt_alpha_beta(64);
    TranspositionTable tt_pvs(64);
    TranspositionTable tt_null_move(64);

    std::vector<std::unique_ptr<Search>> algorithms;
    algorithms.push_back(
        std::make_unique<AlphaBetaSearch>(evaluator, tt_alpha_beta, params));
    algorithms.push_back(std::make_unique<PVS>(evaluator, tt_pvs, params));
    algorithms.push_back(
        std::make_unique<NullMoveSearch>(evaluator, tt_null_move, params));

    for (auto &search : algorithms)
      rows.push_back(
          run_one(std::string(search->name()), *search, spec, position, args));
  }

  print_table(rows);

  std::printf("\n%s\n", std::string(120, '=').c_str());
  std::printf("Aggregate across the full suite:\n\n");

  std::vector<BenchRow> summary;
  for (const char *algo : {"alpha_beta", "pvs", "null_move"})
    summary.push_back(aggregate(algo, rows));
  print_table(summary);

  write_csv(args.csv_path, rows);
  write_csv(timestamped_path(args.csv_path), rows);
  std::printf("\nWrote %s\n", args.csv_path.c_str());

  return 0;
}
