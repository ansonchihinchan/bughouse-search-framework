#include "agent/tournament.h"
#include "agent/observer.h"
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
std::string_view search_name(SearchAlgorithm algorithm) {
  switch (algorithm) {
  case SearchAlgorithm::AlphaBeta:
    return "alpha_beta";
  case SearchAlgorithm::PVS:
    return "pvs";
  case SearchAlgorithm::NullMove:
    return "null_move";
  }
  return "unknown";
}

std::string_view result_name(GameResult result) {
  switch (result) {
  case GameResult::ONGOING:
    return "ongoing";
  case GameResult::TEAM_A_WINS:
    return "team_a_wins";
  case GameResult::TEAM_B_WINS:
    return "team_b_wins";
  case GameResult::DRAW:
    return "draw";
  }
  return "unknown";
}

std::string_view termination_name(SelfPlayTermination termination) {
  switch (termination) {
  case SelfPlayTermination::GameOver:
    return "game_over";
  case SelfPlayTermination::PlyLimit:
    return "ply_limit";
  case SelfPlayTermination::NoLegalMove:
    return "no_legal_move";
  case SelfPlayTermination::InvalidAgentMove:
    return "invalid_agent_move";
  case SelfPlayTermination::Stopped:
    return "stopped";
  }
  return "unknown";
}

int winning_team(GameResult result) {
  if (result == GameResult::TEAM_A_WINS)
    return 0;
  if (result == GameResult::TEAM_B_WINS)
    return 1;
  return -1;
}

void add_to_summary(TournamentSummary &summary,
                    const TournamentGameRecord &record) {
  summary.games++;

  switch (record.result.game_result) {
  case GameResult::TEAM_A_WINS:
    summary.team_a_wins++;
    break;
  case GameResult::TEAM_B_WINS:
    summary.team_b_wins++;
    break;
  case GameResult::DRAW:
    summary.draws++;
    break;
  default:
    summary.unfinished++;
    break;
  }

  summary.total_plies += record.result.plies;
  summary.messages_sent += record.result.messages_sent;
  summary.requests_fulfilled += record.result.requests_fulfilled;
  summary.sacrifices_accepted +=
      record.result.agent_metrics.sacrifices_accepted;
  summary.actual_sacrifice_uses +=
      record.result.accepted_sacrifice_transfers_used;
  summary.coordination_opportunities +=
      record.result.coordination_opportunities;
  summary.coordinated_responses += record.result.coordinated_responses;
  summary.synchrony_credit += record.result.synchrony_credit;
  summary.total_drops += record.result.total_drops;
  summary.wasted_drops += record.result.wasted_drops;
}

std::string pocket_text(const BughousePosition &position) {
  std::ostringstream out;
  for (int player = 0; player < PLAYER_NO; player++) {
    if (player)
      out << '|';
    for (int pt = PAWN; pt <= QUEEN; pt++) {
      if (pt != PAWN)
        out << ':';
      out << position.pockets[player].count(static_cast<PieceType>(pt));
    }
  }
  return out.str();
}

std::string history_text(const BughouseState &state) {
  std::ostringstream out;
  for (size_t i = 0; i < state.history.size(); i++) {
    if (i)
      out << '|';
    const RepetitionNode &node = state.history[i];
    out << node.key << ':' << node.reversible_plies << ':' << node.repetition;
  }
  return out.str();
}

std::string agent_config_text(const AgentConfig &config) {
  const SearchParams &s = config.search_params;
  const TemporalConfig &t = config.temporal_config;
  std::ostringstream out;
  out << agent_type_name(config.type) << ';' << search_name(config.search)
      << ';' << config.transposition_table_mb << ';' << config.seed << ';'
      << s.tt_enabled << ';' << s.see_enabled << ';' << s.see_prune_threshold
      << ';' << s.delta_margin << ';' << s.quiescence_enabled << ';'
      << s.quiescence_max_ply << ';' << s.aspiration_initial_window << ';'
      << s.aspiration_start_depth << ';' << s.aspiration_volatility_scale << ';'
      << s.lmr_enabled << ';' << s.lmr_min_depth << ';'
      << s.lmr_full_depth_moves << ';' << s.lmr_volatility_scale << ';'
      << s.null_move_enabled << ';' << s.null_move_reduction << ';'
      << s.null_move_min_depth << ';' << s.futility_enabled << ';'
      << s.futility_max_depth << ';' << s.futility_base_margin << ';'
      << s.futility_per_depth_margin << ';' << s.futility_volatility_scale
      << ';' << s.age_history << ';' << t.simulated_move_cost_ms << ';'
      << t.rollout_events << ';' << t.rollout_depth << ';' << t.max_candidates
      << ';' << t.local_sacrifice_margin << ';' << t.temporal_gain_margin << ';'
      << t.rollout_tt_mb;
  return out.str();
}
} // namespace

double TournamentSummary::synchrony_score() const {
  if (!coordination_opportunities)
    return 0.0;
  return static_cast<double>(synchrony_credit) /
         (COORDINATION_WINDOW_EVENTS * coordination_opportunities);
}

double TournamentSummary::wasted_drop_rate() const {
  return total_drops ? static_cast<double>(wasted_drops) / total_drops : 0.0;
}

uint64_t tournament_matchup_identity(const ExperimentConfig &matchup) {
  uint64_t identity = 0;
  for (int player = 0; player < PLAYER_NO; player++)
    identity = hash_combine(
        identity,
        static_cast<uint64_t>(matchup.agent_configs[player].type) + 1);
  return identity;
}

uint64_t tournament_game_seed(const ExperimentConfig &matchup,
                              size_t game_index) {
  return hash_combine(
      hash_combine(matchup.seed, tournament_matchup_identity(matchup)),
      static_cast<uint64_t>(game_index));
}

TournamentResult TournamentRunner::run(const TournamentConfig &config,
                                       std::stop_token stop_token) const {
  if (config.game_count == 0)
    throw std::invalid_argument("tournament requires at least one game");

  TournamentResult tournament;
  tournament.config = config;
  tournament.games.reserve(config.game_count);
  if (config.observer)
    config.observer->on_tournament_start(config);

  for (size_t game_index = 0; game_index < config.game_count; game_index++) {
    if (stop_token.stop_requested())
      break;

    uint64_t seed = tournament_game_seed(config.matchup, game_index);
    ExperimentConfig game_experiment = config.matchup;
    game_experiment.seed = seed;
    AgentStrategyExperiment experiment(std::move(game_experiment));
    SelfPlayRunner game_runner(experiment);
    BughouseState game = config.initial_state;
    SelfPlayConfig self_play_config = config.self_play;
    if (config.observer)
      self_play_config.observer = config.observer;
    SelfPlayResult self_play =
        game_runner.run(game, self_play_config, stop_token);

    TournamentGameRecord record;
    record.game_index = game_index;
    record.seed = seed;
    record.winning_team = winning_team(self_play.game_result);
    record.result = std::move(self_play);
    add_to_summary(tournament.summary, record);
    if (config.observer)
      config.observer->on_game_end(record, tournament.summary);
    tournament.games.push_back(std::move(record));
  }
  if (config.observer)
    config.observer->on_tournament_end(tournament);
  return tournament;
}

void write_tournament_csv(std::ostream &out, const TournamentResult &result) {
  out << "game_index,seed,base_seed,matchup_identity,result,winning_team,"
         "termination,plies,";
  for (int player = 0; player < PLAYER_NO; player++)
    out << "agent" << player << "_config,";
  out << "board0_fen,board1_fen,initial_pockets,initial_history,"
         "initial_clock0_ms,"
         "initial_clock1_ms,initial_clock2_ms,initial_clock3_ms,increment_ms,"
         "clock_mode,deterministic_move_time_ms,max_depth,max_nodes,"
         "move_time_ms,search_infinite,"
         "first_board,max_plies,"
         "final_clock0_ms,final_clock1_ms,final_clock2_ms,final_clock3_ms,"
         "moves0,moves1,moves2,moves3,messages,piece_requests,"
         "strategy_requests,fulfilled_requests,piece_transfers,"
         "sacrifice_attempts,sacrifices_accepted,modeled_transfers,"
         "modeled_partner_uses,accepted_sacrifice_transfers,"
         "actual_partner_uses,successful_temporal_sacrifices,"
         "coordination_opportunities,coordinated_responses,synchrony_credit,"
         "synchrony_score,total_drops,wasted_drops,wasted_drop_rate\n";

  const TournamentConfig &config = result.config;
  for (const TournamentGameRecord &game : result.games) {
    const SelfPlayResult &r = game.result;
    out << game.game_index << ',' << game.seed << ',' << config.matchup.seed
        << ',' << tournament_matchup_identity(config.matchup) << ','
        << result_name(r.game_result) << ',' << game.winning_team << ','
        << termination_name(r.termination) << ',' << r.plies << ',';
    for (const AgentConfig &agent : config.matchup.agent_configs)
      out << '"' << agent_config_text(agent) << "\",";
    out << '"' << config.initial_state.position.boards[0].to_fen() << "\",\""
        << config.initial_state.position.boards[1].to_fen() << "\",\""
        << pocket_text(config.initial_state.position) << "\",\""
        << history_text(config.initial_state) << "\",";
    for (int player = 0; player < PLAYER_NO; player++)
      out << config.initial_state.clock.time_ms[player] << ',';
    out << config.initial_state.clock.increment_ms << ','
        << (config.self_play.clock_mode == GameClockMode::RealTime ? "real"
                                                                   : "deterministic")
        << ',' << config.self_play.deterministic_move_time_ms << ','
        << config.self_play.search_limits.max_depth << ','
        << config.self_play.search_limits.max_nodes << ','
        << config.self_play.search_limits.move_time.count() << ','
        << config.self_play.search_limits.infinite << ','
        << config.self_play.first_board << ',' << config.self_play.max_plies
        << ',';
    for (int player = 0; player < PLAYER_NO; player++)
      out << r.final_clocks_ms[player] << ',';
    for (int player = 0; player < PLAYER_NO; player++)
      out << r.moves_by_player[player] << ',';
    out << r.messages_sent << ',' << r.piece_requests_generated << ','
        << r.strategy_requests_generated << ',' << r.requests_fulfilled << ','
        << r.piece_transfers << ',' << r.agent_metrics.sacrifice_attempts << ','
        << r.agent_metrics.sacrifices_accepted << ','
        << r.agent_metrics.temporal_transfers_observed << ','
        << r.agent_metrics.temporal_partner_uses << ','
        << r.accepted_sacrifice_transfers << ','
        << r.accepted_sacrifice_transfers_used << ','
        << r.successful_temporal_sacrifices << ','
        << r.coordination_opportunities << ',' << r.coordinated_responses << ','
        << r.synchrony_credit << ',' << r.synchrony_score << ','
        << r.total_drops << ',' << r.wasted_drops << ',' << r.wasted_drop_rate
        << '\n';
  }
}