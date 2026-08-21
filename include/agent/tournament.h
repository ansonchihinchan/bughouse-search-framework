#pragma once

#include "agent/self_play.h"
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <stop_token>
#include <vector>

class TournamentObserver;

struct TournamentConfig {
  ExperimentConfig matchup{};
  BughouseState initial_state{};
  SelfPlayConfig self_play{};
  size_t game_count = 1;
  TournamentObserver *observer = nullptr;
};

struct TournamentGameRecord {
  size_t game_index = 0;
  uint64_t seed = 0;
  int winning_team = -1; // -1: none/draw, 0: team A, 1: team B
  SelfPlayResult result{};
};

struct TournamentSummary {
  size_t games = 0;
  size_t team_a_wins = 0;
  size_t team_b_wins = 0;
  size_t draws = 0;
  size_t unfinished = 0;
  size_t total_plies = 0;
  size_t messages_sent = 0;
  size_t requests_fulfilled = 0;
  size_t sacrifices_accepted = 0;
  size_t actual_sacrifice_uses = 0;
  size_t coordination_opportunities = 0;
  size_t coordinated_responses = 0;
  size_t synchrony_credit = 0;
  size_t total_drops = 0;
  size_t wasted_drops = 0;

  double synchrony_score() const;
  double wasted_drop_rate() const;
};

struct TournamentResult {
  TournamentConfig config{};
  std::vector<TournamentGameRecord> games;
  TournamentSummary summary{};
};

uint64_t tournament_matchup_identity(const ExperimentConfig &matchup);
uint64_t tournament_game_seed(const ExperimentConfig &matchup,
                              size_t game_index);

class TournamentRunner {
public:
  TournamentResult run(const TournamentConfig &config,
                       std::stop_token stop_token = {}) const;
};

void write_tournament_csv(std::ostream &out, const TournamentResult &result);