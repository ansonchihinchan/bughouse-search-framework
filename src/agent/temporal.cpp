#include "agent/temporal.h"

#include "game/movegen.h"
#include "game/piece_value.h"
#include "search/pvs.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
GameResult winner_for_flagged_team(bool team_a_flagged, bool team_b_flagged) {
  if (team_a_flagged && team_b_flagged)
    return GameResult::DRAW;
  if (team_a_flagged)
    return GameResult::TEAM_B_WINS;
  if (team_b_flagged)
    return GameResult::TEAM_A_WINS;
  return GameResult::ONGOING;
}

bool team_a(PlayerId player) {
  return player == to_player(0) || player == to_player(2);
}

int terminal_score(GameResult result, PlayerId root_player) {
  if (result == GameResult::DRAW)
    return DRAW_SCORE;
  bool root_team_a = team_a(root_player);
  bool root_won = (result == GameResult::TEAM_A_WINS && root_team_a) ||
                  (result == GameResult::TEAM_B_WINS && !root_team_a);
  return root_won ? INF_SCORE : -INF_SCORE;
}

bool deadline_reached(const SearchLimits &limits,
                      std::chrono::steady_clock::time_point start) {
  return limits.move_time.count() > 0 &&
         std::chrono::steady_clock::now() - start >= limits.move_time;
}
} // namespace

bool TemporalTrace::append(const TemporalEvent &event) {
  if (event_count >= events.size())
    return false;
  events[event_count++] = event;
  return true;
}

std::optional<PlayerId>
TemporalScheduler::next_actor(const TemporalState &state) const {
  for (int offset = 0; offset < BOARD_NO; offset++) {
    int board = (state.preferred_board + offset) % BOARD_NO;
    PlayerId actor =
        player_on_board(board, state.position.boards[board].sideToMove);
    if (!generate_legal_moves(state.position, actor).empty())
      return actor;
  }
  return std::nullopt;
}

GameResult TemporalScheduler::advance_clocks(TemporalState &state,
                                             PlayerId actor) const {
  std::array<PlayerId, BOARD_NO> active{};
  for (int board = 0; board < BOARD_NO; board++) {
    active[board] =
        player_on_board(board, state.position.boards[board].sideToMove);
    state.remaining_ms[to_int(active[board])] -= config_.simulated_move_cost_ms;
  }

  bool team_a_flagged = false;
  bool team_b_flagged = false;
  for (PlayerId player : active) {
    if (state.remaining_ms[to_int(player)] > 0)
      continue;
    if (team_a(player))
      team_a_flagged = true;
    else
      team_b_flagged = true;
  }

  return winner_for_flagged_team(team_a_flagged, team_b_flagged);
}

bool TemporalScheduler::apply_event(TemporalState &state, PlayerId actor,
                                    Move move, TemporalTrace &trace) const {
  if (board_of(actor) < 0 || board_of(actor) >= BOARD_NO ||
      actor !=
          player_on_board(board_of(actor),
                          state.position.boards[board_of(actor)].sideToMove) ||
      !state.position.boards[board_of(actor)].is_legal(move))
    throw std::invalid_argument("illegal temporal event");

  TemporalEvent event;
  event.actor = actor;
  event.move = move;
  event.elapsed_ms = config_.simulated_move_cost_ms;

  GameResult flag_result = advance_clocks(state, actor);
  if (flag_result != GameResult::ONGOING) {
    trace.game_result = flag_result;
    trace.stop_reason = TemporalStopReason::GameOver;
    return false;
  }

  BughouseUndo undo = apply_move(state.position, actor, move);
  state.remaining_ms[to_int(actor)] += state.increment_ms;
  if (undo.creditedPartner)
    event.transferred_piece = undo.creditedPiece;

  if (move.is_drop() && actor == partner_of(state.root_player) &&
      move.drop_pt == state.transferred_piece_type &&
      state.transfer_still_available) {
    event.tracked_transfer_consumed = true;
    trace.partner_used_transfer = true;
    state.transfer_still_available = false;
  }

  trace.append(event);
  state.preferred_board = 1 - board_of(actor);
  if (state.events_remaining > 0)
    state.events_remaining--;
  return true;
}

GameResult
TemporalScheduler::position_result(const TemporalState &state) const {
  for (int board = 0; board < BOARD_NO; board++) {
    if (!state.position.boards[board].is_checkmate())
      continue;
    PlayerId loser =
        player_on_board(board, state.position.boards[board].sideToMove);
    return team_a(loser) ? GameResult::TEAM_B_WINS : GameResult::TEAM_A_WINS;
  }
  return GameResult::ONGOING;
}

struct TemporalCoordinator::Impl {
  struct Candidate {
    Move move;
    PieceType transfer = NO_PIECE_TYPE;
    bool causal =
        false; // whether transfer is already available in the partner's pocket
    int rank = 0;
  };

  explicit Impl(const TemporalConfig &config)
      : rollout_evaluator(BughouseEvaluationConfig::independent()),
        team_evaluator(BughouseEvaluationConfig::shared_value()) {
    params.tt_enabled = true;

    params.quiescence_enabled = false;
    for (int i = 0; i < PLAYER_NO; i++) {
      tables[i] = std::make_unique<TranspositionTable>(config.rollout_tt_mb);
      policies[i] =
          std::make_unique<PVS>(rollout_evaluator, *tables[i], params);
    }
  }

  BughouseEvaluator rollout_evaluator;
  BughouseEvaluator team_evaluator;
  SearchParams params{};
  std::array<std::unique_ptr<TranspositionTable>, PLAYER_NO> tables;
  std::array<std::unique_ptr<PVS>, PLAYER_NO> policies;
  CommunicationContext empty_communication{};

  SearchResult policy_search(const BughousePosition &position,
                             const std::array<int64_t, PLAYER_NO> &remaining,
                             PlayerId actor, int depth,
                             std::stop_token stop_token,
                             TemporalDecisionStats &stats) {
    stats.local_search_calls++;
    BughousePosition probe = position;
    for (Move move : generate_legal_moves(position, actor)) {
      if (stop_token.stop_requested())
        break;
      BughouseUndo undo = apply_move(probe, actor, move);
      bool mate = probe.boards[board_of(actor)].is_checkmate();
      undo_move(probe, actor, move, undo);
      if (mate) {
        SearchResult result;
        result.best_move = move;
        result.score = INF_SCORE;
        result.depth = depth;
        result.pv = {move};
        result.completed = true;
        return result;
      }
    }
    SearchLimits limits;
    limits.max_depth = depth;
    SearchContext context =
        make_context(remaining, actor, empty_communication,
                     communication_hash(empty_communication));
    return policies[to_int(actor)]->search(position, context, limits,
                                           stop_token);
  }

  int local_score(const BughousePosition &position,
                  const std::array<int64_t, PLAYER_NO> &remaining,
                  PlayerId root_player, Move move, int depth,
                  std::stop_token stop_token, TemporalDecisionStats &stats) {
    BughousePosition after = position;
    apply_move(after, root_player, move);
    PlayerId opponent = next_player(root_player);
    if (generate_legal_moves(after, opponent).empty()) {
      return after.boards[board_of(opponent)].is_in_check() ? INF_SCORE : 0;
    }
    SearchResult reply =
        policy_search(after, remaining, opponent, depth, stop_token, stats);
    return -reply.score;
  }

  std::vector<Candidate> candidates(const BughousePosition &position,
                                    PlayerId root_player, Move baseline,
                                    size_t maximum) const {
    std::vector<Candidate> result;
    result.reserve(maximum);
    result.push_back(Candidate{baseline});

    const Board &board = position.boards[board_of(root_player)];
    PlayerId recipient = partner_of(root_player);
    std::vector<Candidate> transfers;
    BughousePosition probe = position;
    for (Move move : generate_legal_moves(position, root_player)) {
      if (move == baseline || !board.is_capture(move))
        continue;
      BughouseUndo undo = apply_move(probe, root_player, move);
      PieceType transfer = undo.creditedPiece;
      bool transferred = undo.creditedPartner;
      undo_move(probe, root_player, move, undo);
      if (!transferred || transfer == NO_PIECE_TYPE || transfer == KING)
        continue;
      PieceType mover =
          move.is_drop() ? NO_PIECE_TYPE : board.piece_on(move.from).type;
      int rank = 2 * PieceValue::effective_value(transfer) -
                 PieceValue::effective_value(mover);
      bool causal = !position.pockets[to_int(recipient)].contains(transfer);
      transfers.push_back(Candidate{move, transfer, causal, rank});
    }

    std::stable_sort(transfers.begin(), transfers.end(),
                     [](const Candidate &a, const Candidate &b) {
                       if (a.causal != b.causal)
                         return a.causal > b.causal;
                       return a.rank > b.rank;
                     });
    for (const Candidate &candidate : transfers) {
      if (result.size() >= maximum)
        break;
      result.push_back(candidate);
    }
    return result;
  }

  TemporalTrace rollout(const BughouseState &game, PlayerId root_player,
                        Move root_move, PieceType tracked_transfer,
                        bool track_transfer, const TemporalConfig &config,
                        const SearchLimits &limits, std::stop_token stop_token,
                        std::chrono::steady_clock::time_point decision_start,
                        TemporalDecisionStats &stats) {
    TemporalState state;
    state.position = game.position;
    for (int i = 0; i < PLAYER_NO; i++)
      state.remaining_ms[i] = game.clock.remaining(to_player(i));
    state.preferred_board = board_of(root_player);
    state.events_remaining = config.rollout_events + 1;
    state.root_player = root_player;
    state.increment_ms = game.clock.increment_ms;

    TemporalTrace trace;
    TemporalScheduler scheduler(config);
    if (!scheduler.apply_event(state, root_player, root_move, trace)) {
      trace.team_score = terminal_score(trace.game_result, root_player);
      return trace;
    }
    stats.rollout_events++;

    if (track_transfer) {
      state.transferred_piece_type = tracked_transfer;
      state.transfer_still_available = true;
    }

    while (state.events_remaining > 0) {
      if (stop_token.stop_requested() ||
          deadline_reached(limits, decision_start)) {
        trace.stop_reason = TemporalStopReason::Interrupted;
        break;
      }

      trace.game_result = scheduler.position_result(state);
      if (trace.game_result != GameResult::ONGOING) {
        trace.stop_reason = TemporalStopReason::GameOver;
        break;
      }

      std::optional<PlayerId> actor = scheduler.next_actor(state);
      if (!actor) {
        trace.stop_reason = TemporalStopReason::NoLegalEvent;
        break;
      }

      SearchResult choice =
          policy_search(state.position, state.remaining_ms, *actor,
                        config.rollout_depth, stop_token, stats);
      if (choice.best_move.is_none()) {
        trace.stop_reason = TemporalStopReason::NoLegalEvent;
        break;
      }
      if (!scheduler.apply_event(state, *actor, choice.best_move, trace))
        break;
      stats.rollout_events++;
    }

    if (trace.game_result == GameResult::ONGOING)
      trace.game_result = scheduler.position_result(state);
    if (trace.game_result != GameResult::ONGOING)
      trace.team_score = terminal_score(trace.game_result, root_player);
    else
      trace.team_score = team_evaluator.evaluate(
          state.position, root_player, state.remaining_ms, empty_communication);
    return trace;
  }
};

bool qualifies_sacrifice(const SacrificeEvidence &evidence,
                         const TemporalConfig &config) {
  return evidence.resource_transferred && evidence.causal_availability &&
         evidence.partner_used_transfer &&
         evidence.candidate_local_score <=
             evidence.baseline_local_score - config.local_sacrifice_margin &&
         evidence.candidate_team_score >=
             evidence.baseline_team_score + config.temporal_gain_margin;
}

TemporalCoordinator::TemporalCoordinator(TemporalConfig config)
    : config_(std::move(config)) {
  if (config_.simulated_move_cost_ms <= 0 || config_.rollout_events < 0 ||
      config_.rollout_events + 1 > static_cast<int>(MAX_TEMPORAL_EVENTS) ||
      config_.rollout_depth <= 0 || config_.max_candidates == 0 ||
      config_.max_candidates > MAX_TEMPORAL_CANDIDATES)
    throw std::invalid_argument("invalid temporal configuration");
  impl_ = std::make_unique<Impl>(config_);
}

TemporalCoordinator::~TemporalCoordinator() = default;

SearchResult TemporalCoordinator::select_move(
    const BughouseState &game, PlayerId root_player,
    const SearchResult &baseline, const SearchLimits &limits,
    std::stop_token stop_token,
    std::chrono::steady_clock::time_point decision_start) {
  last_decision_ = TemporalDecision{};
  last_decision_.result = baseline;
  const auto temporal_start = std::chrono::steady_clock::now();

  if (baseline.best_move.is_none() || stop_token.stop_requested() ||
      deadline_reached(limits, decision_start))
    return baseline;

  std::array<int64_t, PLAYER_NO> remaining{};
  for (int i = 0; i < PLAYER_NO; i++)
    remaining[i] = game.clock.remaining(to_player(i));

  std::vector<Impl::Candidate> candidates = impl_->candidates(
      game.position, root_player, baseline.best_move, config_.max_candidates);
  last_decision_.stats.candidate_count = candidates.size();
  last_decision_.baseline_local_score = impl_->local_score(
      game.position, remaining, root_player, baseline.best_move,
      config_.rollout_depth, stop_token, last_decision_.stats);
  last_decision_.selected_local_score = last_decision_.baseline_local_score;
  last_decision_.baseline_trace = impl_->rollout(
      game, root_player, baseline.best_move, NO_PIECE_TYPE, false, config_,
      limits, stop_token, decision_start, last_decision_.stats);
  last_decision_.selected_trace = last_decision_.baseline_trace;

  if (last_decision_.baseline_trace.stop_reason ==
          TemporalStopReason::Interrupted ||
      stop_token.stop_requested() || deadline_reached(limits, decision_start))
    return baseline;

  int best_temporal = last_decision_.baseline_trace.team_score;
  for (size_t i = 1; i < candidates.size(); i++) {
    if (stop_token.stop_requested() || deadline_reached(limits, decision_start))
      break;
    const Impl::Candidate &candidate = candidates[i];
    if (!candidate.causal)
      continue;
    last_decision_.stats.sacrifice_candidates_considered++;

    int local = impl_->local_score(game.position, remaining, root_player,
                                   candidate.move, config_.rollout_depth,
                                   stop_token, last_decision_.stats);
    if (stop_token.stop_requested() || deadline_reached(limits, decision_start))
      break;
    if (local >
        last_decision_.baseline_local_score - config_.local_sacrifice_margin)
      continue;

    TemporalTrace trace = impl_->rollout(
        game, root_player, candidate.move, candidate.transfer, true, config_,
        limits, stop_token, decision_start, last_decision_.stats);
    last_decision_.last_candidate_trace = trace;
    last_decision_.last_candidate_local_score = local;
    if (trace.stop_reason == TemporalStopReason::Interrupted)
      break;
    if (trace.event_count > 0 &&
        trace.events[0].transferred_piece == candidate.transfer)
      last_decision_.stats.transfers_observed++;
    if (trace.partner_used_transfer)
      last_decision_.stats.transferred_pieces_used++;
    SacrificeEvidence evidence{
        last_decision_.baseline_local_score,
        local,
        last_decision_.baseline_trace.team_score,
        trace.team_score,
        trace.event_count > 0 &&
            trace.events[0].transferred_piece == candidate.transfer,
        candidate.causal,
        trace.partner_used_transfer,
    };
    if (!qualifies_sacrifice(evidence, config_))
      continue;
    if (!last_decision_.selected_sacrifice ||
        trace.team_score > best_temporal) {
      best_temporal = trace.team_score;
      last_decision_.selected_sacrifice = true;
      last_decision_.selected_local_score = local;
      last_decision_.selected_trace = trace;
      last_decision_.result = baseline;
      last_decision_.result.best_move = candidate.move;
      last_decision_.result.score = trace.team_score;
      last_decision_.result.pv = {candidate.move};
    }
  }

  if (stop_token.stop_requested() || deadline_reached(limits, decision_start)) {
    last_decision_.selected_sacrifice = false;
    last_decision_.result = baseline;
  }
  last_decision_.stats.elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - temporal_start);
  return last_decision_.result;
}