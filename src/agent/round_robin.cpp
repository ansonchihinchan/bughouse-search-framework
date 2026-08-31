#include "agent/round_robin.h"
#include "agent/observer.h"
#include <sstream>
#include <stdexcept>

namespace {
class GameCallbackForwarder final : public TournamentObserver {
public:
  explicit GameCallbackForwarder(TournamentObserver &target)
      : target_(target) {}

  void on_game_start(const BughouseState &state) override {
    target_.on_game_start(state);
  }
  void on_ply(const ReplayEvent &event, const BughouseState &state) override {
    target_.on_ply(event, state);
  }
  void on_game_end(const SelfPlayResult &result) override {
    static_cast<Observer &>(target_).on_game_end(result);
  }

private:
  TournamentObserver &target_;
};
} // namespace

std::vector<ExperimentConfig>
generate_matchup_schedule(const RoundRobinConfig &config) {
  if (config.mode == ScheduleMode::Explicit)
    return config.explicit_matchups;

  if (config.agent_types.empty())
    throw std::invalid_argument("schedule requires at least one agent type");

  std::vector<ExperimentConfig> schedule;
  if (config.mode == ScheduleMode::Homogeneous) {
    schedule.reserve(config.agent_types.size());
    for (AgentType type : config.agent_types) {
      ExperimentConfig matchup = config.tournament.matchup;
      for (AgentConfig &agent : matchup.agent_configs)
        agent.type = type;
      schedule.push_back(std::move(matchup));
    }
    return schedule;
  }

  size_t count = config.agent_types.size();
  schedule.reserve(count * count * count * count);
  for (AgentType p0 : config.agent_types)
    for (AgentType p1 : config.agent_types)
      for (AgentType p2 : config.agent_types)
        for (AgentType p3 : config.agent_types) {
          ExperimentConfig matchup = config.tournament.matchup;
          matchup.agent_configs[0].type = p0;
          matchup.agent_configs[1].type = p1;
          matchup.agent_configs[2].type = p2;
          matchup.agent_configs[3].type = p3;
          schedule.push_back(std::move(matchup));
        }
  return schedule;
}

RoundRobinResult RoundRobinRunner::run(const RoundRobinConfig &config,
                                       std::stop_token stop_token) const {
  RoundRobinResult result;
  result.schedule = generate_matchup_schedule(config);
  result.matchups.reserve(result.schedule.size());
  result.games.reserve(result.schedule.size() * config.tournament.game_count);
  TournamentRunner runner;
  TournamentObserver *observer = config.tournament.observer;
  TournamentConfig aggregate_config = config.tournament;
  aggregate_config.game_count =
      result.schedule.size() * config.tournament.game_count;
  if (observer)
    observer->on_tournament_start(aggregate_config);
  std::optional<GameCallbackForwarder> game_observer;
  if (observer)
    game_observer.emplace(*observer);

  for (const ExperimentConfig &matchup : result.schedule) {
    if (stop_token.stop_requested())
      break;
    TournamentConfig tournament = config.tournament;
    tournament.matchup = matchup;
    tournament.observer = game_observer ? &*game_observer : nullptr;
    TournamentResult matchup_result = runner.run(tournament, stop_token);
    for (const TournamentGameRecord &matchup_game : matchup_result.games) {
      TournamentGameRecord game = matchup_game;
      accumulate_tournament_game(result.summary, game);
      result.games.push_back(game);
      if (observer)
        observer->on_game_end(result.games.back(), result.summary);
    }
    result.matchups.push_back(std::move(matchup_result));
  }
  if (observer) {
    TournamentResult aggregate_result;
    aggregate_result.config = aggregate_config;
    aggregate_result.games = result.games;
    aggregate_result.summary = result.summary;
    observer->on_tournament_end(aggregate_result);
  }
  return result;
}

void write_round_robin_csv(std::ostream &out, const RoundRobinResult &result) {
  bool first = true;
  for (const TournamentResult &matchup : result.matchups) {
    std::ostringstream csv;
    write_tournament_csv(csv, matchup);
    std::string text = csv.str();
    if (!first) {
      size_t newline = text.find('\n');
      text.erase(0, newline == std::string::npos ? text.size() : newline + 1);
    }
    out << text;
    first = false;
  }
}