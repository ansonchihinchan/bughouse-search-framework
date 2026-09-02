#include "agent/observer.h"
#include "agent/replay.h"
#include "agent/round_robin.h"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
struct Options {
  uint64_t seed = 0;
  size_t games = 1;
  size_t max_plies = 32;
  int depth = 1;
  uint64_t max_nodes = 0;
  int time_ms = 0;
  int initial_time_seconds = 180;
  int increment_seconds = 2;
  int64_t deterministic_move_time_ms = 1000;
  SearchAlgorithm algorithm = SearchAlgorithm::PVS;
  std::array<AgentType, PLAYER_NO> agents = {
      AgentType::Independent, AgentType::Independent, AgentType::Independent,
      AgentType::Independent};
  ScheduleMode schedule = ScheduleMode::Homogeneous;
  std::string output;
  bool step = false;
  bool live = false;
  bool clock_mode_set = false;
  GameClockMode clock_mode = GameClockMode::RealTime;
};

AgentType parse_agent_type(std::string_view name) {
  if (name == "independent")
    return AgentType::Independent;
  if (name == "request")
    return AgentType::Request;
  if (name == "shared_value")
    return AgentType::SharedValue;
  if (name == "sacrifice")
    return AgentType::Sacrifice;

  throw std::invalid_argument("agent must be one of: independent, "
                              "request shared_value or sacrifice");
}

std::string next_value(int &index, int argc, char **argv) {
  if (++index >= argc)
    throw std::invalid_argument("missing option value");
  return argv[index];
}

Options parse_options(int start, int argc, char **argv) {
  Options options;
  for (int i = start; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--seed")
      options.seed = std::stoull(next_value(i, argc, argv));
    else if (arg == "--games")
      options.games = std::stoull(next_value(i, argc, argv));
    else if (arg == "--max-plies")
      options.max_plies = std::stoull(next_value(i, argc, argv));
    else if (arg == "--depth")
      options.depth = std::stoi(next_value(i, argc, argv));
    else if (arg == "--max-nodes")
      options.max_nodes = std::stoull(next_value(i, argc, argv));
    else if (arg == "--time-ms")
      options.time_ms = std::stoi(next_value(i, argc, argv));
    else if (arg == "--time")
      options.initial_time_seconds = std::stoi(next_value(i, argc, argv));
    else if (arg == "--increment")
      options.increment_seconds = std::stoi(next_value(i, argc, argv));
    else if (arg == "--deterministic-move-ms")
      options.deterministic_move_time_ms =
          std::stoll(next_value(i, argc, argv));
    else if (arg == "--output")
      options.output = next_value(i, argc, argv);
    else if (arg == "--agents")
      options.agents.fill(parse_agent_type(next_value(i, argc, argv)));
    else if (arg == "--agent-0")
      options.agents[0] = parse_agent_type(next_value(i, argc, argv));
    else if (arg == "--agent-1")
      options.agents[1] = parse_agent_type(next_value(i, argc, argv));
    else if (arg == "--agent-2")
      options.agents[2] = parse_agent_type(next_value(i, argc, argv));
    else if (arg == "--agent-3")
      options.agents[3] = parse_agent_type(next_value(i, argc, argv));
    else if (arg == "--step")
      options.step = true;
    else if (arg == "--live")
      options.live = true;
    else if (arg == "--clock") {
      std::string mode = next_value(i, argc, argv);
      if (mode == "real")
        options.clock_mode = GameClockMode::RealTime;
      else if (mode == "deterministic")
        options.clock_mode = GameClockMode::Deterministic;
      else
        throw std::invalid_argument("clock must be real or deterministic");
      options.clock_mode_set = true;
    } else if (arg == "--mode") {
      std::string mode = next_value(i, argc, argv);
      if (mode == "homogeneous")
        options.schedule = ScheduleMode::Homogeneous;
      else if (mode == "exhaustive")
        options.schedule = ScheduleMode::ExhaustiveOrdered;
      else
        throw std::invalid_argument("mode must be homogeneous or exhaustive");
    } else if (arg == "--algorithm") {
      std::string algorithm = next_value(i, argc, argv);
      if (algorithm == "alpha_beta")
        options.algorithm = SearchAlgorithm::AlphaBeta;
      else if (algorithm == "pvs")
        options.algorithm = SearchAlgorithm::PVS;
      else if (algorithm == "null_move")
        options.algorithm = SearchAlgorithm::NullMove;
      else
        throw std::invalid_argument("unknown search algorithm");
    } else {
      throw std::invalid_argument("unknown option: " + arg);
    }
  }
  return options;
}

ExperimentConfig roster(const Options &options) {
  ExperimentConfig config;
  config.seed = options.seed;
  for (int p = 0; p < PLAYER_NO; p++) {
    AgentConfig &agent = config.agent_configs[p];
    agent.type = options.agents[p];
    agent.search = options.algorithm;
  }
  return config;
}

SelfPlayConfig game_config(const Options &options,
                           GameClockMode default_clock_mode) {
  SelfPlayConfig config;
  config.max_plies = options.max_plies;
  config.search_limits.max_depth = options.depth;
  config.search_limits.max_nodes = options.max_nodes;
  config.search_limits.move_time = std::chrono::milliseconds(options.time_ms);
  config.clock_mode =
      options.clock_mode_set ? options.clock_mode : default_clock_mode;
  config.deterministic_move_time_ms = options.deterministic_move_time_ms;
  return config;
}

void print_usage() {
  std::cout << "Usage:\n"
               "  bughouse self-play [--agents "
               "independent|request|shared_value|sacrifice] [--agent-<0-3> "
               "TYPE] [--seed N] "
               "[--depth N] [--max-plies N] "
               "[--clock real|deterministic] [--time SEC] [--increment SEC] "
               "[--output game.replay] [--live]\n"
               "  bughouse tournament [--games N] [--seed N] "
               "[--mode homogeneous|exhaustive] [--algorithm pvs|alpha_beta|"
               "null_move] [--depth N] [--max-nodes N] [--time-ms N] "
               "[--clock real|deterministic] [--time SEC] [--increment SEC] "
               "[--output results.csv] [--live]\n"
               "  bughouse replay <file> [--step]\n";
}

int self_play_command(const Options &options) {
  AgentStrategyExperiment experiment(roster(options));
  SelfPlayRunner runner(experiment);
  BughouseState game;
  game.clock.set(static_cast<int64_t>(options.initial_time_seconds) * 1000,
                 options.increment_seconds * 1000);
  SelfPlayConfig config = game_config(options, GameClockMode::RealTime);
  TerminalObserver observer(std::cout, options.live);
  if (options.live)
    config.observer = &observer;
  SelfPlayResult result = runner.run(game, config);
  std::cout << "plies=" << result.plies
            << " result=" << static_cast<int>(result.game_result)
            << " synchrony=" << result.synchrony_score
            << " wasted_drop_rate=" << result.wasted_drop_rate << '\n';
  if (!options.output.empty()) {
    std::ofstream out(options.output);
    if (!out)
      throw std::runtime_error("cannot open replay output");
    write_replay(out, result.replay);
  }
  return 0;
}

int tournament_command(const Options &options) {
  RoundRobinConfig config;
  config.mode = options.schedule;
  config.tournament.matchup = roster(options);
  config.tournament.game_count = options.games;
  config.tournament.self_play =
      game_config(options, GameClockMode::Deterministic);
  config.tournament.initial_state.clock.set(
      static_cast<int64_t>(options.initial_time_seconds) * 1000,
      options.increment_seconds * 1000);
  TerminalObserver observer(std::cout, options.live);
  if (options.live)
    config.tournament.observer = &observer;
  RoundRobinResult result = RoundRobinRunner{}.run(config);

  std::ostream *destination = options.live ? nullptr : &std::cout;
  std::ofstream file;
  if (!options.output.empty()) {
    file.open(options.output);
    if (!file)
      throw std::runtime_error("cannot open tournament output");
    destination = &file;
  }
  if (destination)
    write_round_robin_csv(*destination, result);
  std::cerr << "matchups=" << result.matchups.size() << '\n';
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  try {
    if (argc < 2) {
      print_usage();
      return 0;
    }
    std::string command = argv[1];
    if (command == "self-play")
      return self_play_command(parse_options(2, argc, argv));
    if (command == "tournament")
      return tournament_command(parse_options(2, argc, argv));
    if (command == "replay") {
      if (argc < 3)
        throw std::invalid_argument("replay requires a file");
      std::ifstream input(argv[2]);
      if (!input)
        throw std::runtime_error("cannot open replay file");
      GameReplay replay = read_replay(input);
      bool step = argc > 3 && std::string_view(argv[3]) == "--step";
      view_replay(std::cout, replay, step, step ? &std::cin : nullptr);
      return 0;
    }
    print_usage();
    return 1;
  } catch (const std::exception &error) {
    std::cerr << "error: " << error.what() << '\n';
    return 2;
  }
}