#include <catch2/catch_all.hpp>

#include "eval/evaluator.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include "search/alpha_beta_search.h"
#include "search/null_move_search.h"
#include "search/pvs.h"
#include "search/see.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <stop_token>

namespace {

// Minimal team-relative material evaluator
class MaterialEvaluator : public Evaluator {
public:
  int evaluate(const BughousePosition &position, PlayerId root_player,
               const std::array<int64_t, PLAYER_NO> &remaining,
               const CommunicationContext &comm_context) const override {
    int score = 0;
    for (int b = 0; b < BOARD_NO; b++) {
      const Board &board = position.boards[b];
      Colour ours = team_colour(root_player, b);
      Colour theirs = flip(ours);
      for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
        score += SEE::PIECE_VALUE[pt] *
                 (std::popcount(board.bitboard_piece(make_piece(ours, pt))) -
                  std::popcount(board.bitboard_piece(make_piece(theirs, pt))));
      }
    }
    return score;
  }
};

bool is_legal_move(const std::vector<Move> &moves, const Move &m) {
  return std::find(moves.begin(), moves.end(), m) != moves.end();
}

// Position where White (player 0) has Rd1xd2 winning the black queen for free
BughousePosition free_queen_position() {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/3q4/3RK3 w - - 0 1");
  return pos;
}

} // namespace

TEST_CASE("AlphaBetaSearch::name", "[search][tree_search]") {
  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);
  REQUIRE(search.name() == "alpha_beta");
}

TEST_CASE("PVS::name", "[search][tree_search]") {
  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  PVS search(eval, tt, params);
  REQUIRE(search.name() == "pvs");
}

TEST_CASE("NullMoveSearch::name", "[search][tree_search]") {
  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  NullMoveSearch search(eval, tt, params);
  REQUIRE(search.name() == "null_move");
}

TEST_CASE("AlphaBetaSearch finds undefended queen capture",
          "[search][tree_search]") {
  BughousePosition pos = free_queen_position();
  BughouseClock clock = make_clock();
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits;
  limits.max_depth = 3;

  std::stop_source src;
  SearchResult result = search.search(pos, context, limits, src.get_token());

  Move expected = Move::normal(to_square(3, 0), to_square(3, 1)); // Rd1xd2
  REQUIRE(result.best_move == expected);
  REQUIRE(result.score > 0);
}

TEST_CASE("PVS finds undefended queen capture", "[search][tree_search]") {
  BughousePosition pos = free_queen_position();
  BughouseClock clock = make_clock();
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  PVS search(eval, tt, params);

  SearchLimits limits;
  limits.max_depth = 3;

  std::stop_source src;
  SearchResult result = search.search(pos, context, limits, src.get_token());

  Move expected = Move::normal(to_square(3, 0), to_square(3, 1));
  REQUIRE(result.best_move == expected);
  REQUIRE(result.score > 0);
}

TEST_CASE("NullMoveSearch finds same undefended queen capture",
          "[search][tree_search]") {
  BughousePosition pos = free_queen_position();
  BughouseClock clock = make_clock();
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  NullMoveSearch search(eval, tt, params);

  SearchLimits limits;
  limits.max_depth = 3;

  std::stop_source src;
  SearchResult result = search.search(pos, context, limits, src.get_token());

  Move expected = Move::normal(to_square(3, 0), to_square(3, 1));
  REQUIRE(result.best_move == expected);
  REQUIRE(result.score > 0);
}

TEST_CASE("search() returns a legal move from the standard start position",
          "[search][tree_search]") {
  BughouseState game;
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(game.clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits;
  limits.max_depth = 2;

  std::stop_source src;
  SearchResult result =
      search.search(game.position, context, limits, src.get_token());

  auto legal_moves = generate_legal_moves(game.position, to_player(0));
  REQUIRE(is_legal_move(legal_moves, result.best_move));
}

TEST_CASE("search() honours max_depth and does not search beyond it",
          "[search][tree_search]") {
  BughouseState game;
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(game.clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits;
  limits.max_depth = 1;

  std::stop_source src;
  SearchResult result =
      search.search(game.position, context, limits, src.get_token());

  REQUIRE(result.stats.depth_reached == 1);
}

TEST_CASE("search() stops once max_nodes is reached and does not keep "
          "iterating to unbounded depth",
          "[search][tree_search]") {
  BughouseState game;
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(game.clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits;
  limits.max_nodes = 50;

  std::stop_source src;
  SearchResult result =
      search.search(game.position, context, limits, src.get_token());

  REQUIRE(result.stats.nodes >= 50);
  // Generous upper bound
  REQUIRE(result.stats.nodes < 5000);
}

TEST_CASE("search() stops within a small requested move_time budget",
          "[search][tree_search]") {
  BughouseState game;
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(game.clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits;
  limits.move_time = std::chrono::milliseconds(20);

  std::stop_source src;
  auto start = std::chrono::steady_clock::now();
  SearchResult result =
      search.search(game.position, context, limits, src.get_token());
  auto elapsed = std::chrono::steady_clock::now() - start;

  // Generous bound
  REQUIRE(elapsed < std::chrono::seconds(5));
  REQUIRE_FALSE(result.best_move.is_none());
}

TEST_CASE("search() halts immediately when the stop_token is already "
          "cancelled",
          "[search][tree_search]") {
  BughouseState game;
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(game.clock, to_player(0), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits; // unbounded

  std::stop_source src;
  src.request_stop();

  SearchResult result =
      search.search(game.position, context, limits, src.get_token());

  REQUIRE(result.best_move.is_none());
  REQUIRE(result.stats.depth_reached == 0);
}

TEST_CASE("search() reports no move and a zero score for an already "
          "checkmated root position",
          "[search][tree_search]") {
  // Black to move and checkmated on board 0. Player 1 is Black on board 0.
  BughousePosition pos;
  pos.boards[0].load_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
  BughouseClock clock = make_clock();
  CommunicationContext comm_context = CommunicationContext{};
  SearchContext context = make_context(clock, to_player(1), comm_context);

  MaterialEvaluator eval;
  TranspositionTable tt(64);
  SearchParams params;
  AlphaBetaSearch search(eval, tt, params);

  SearchLimits limits;
  limits.max_depth = 3;

  std::stop_source src;
  SearchResult result = search.search(pos, context, limits, src.get_token());

  REQUIRE(result.best_move.is_none());
  REQUIRE(result.score == 0);
  REQUIRE(result.stats.depth_reached == 0);
}