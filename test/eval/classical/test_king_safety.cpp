#include <catch2/catch_all.hpp>

#include "eval/classical/king_safety.h"
#include "eval/types.h"
#include "game/bughouse.h"
#include "search/see.h"

namespace {
constexpr int ATTACK_UNIT_PENALTY = 8;
constexpr int SHIELD_BONUS = 6;
} // namespace

TEST_CASE("KingSafetyEvaluator scores bare kings with no pockets as zero",
          "[eval][king_safety]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  KingSafetyEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("KingSafetyEvaluator rewards a pawn shield in the king's own zone",
          "[eval][king_safety]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  KingSafetyEvaluator eval;

  BughousePosition no_shield;
  no_shield.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  no_shield.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  BughousePosition shielded;
  shielded.boards[0].load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"); // e2 shield
  shielded.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  int no_shield_score = eval.evaluate(to_context(no_shield, search)).mid_game();
  int shielded_score = eval.evaluate(to_context(shielded, search)).mid_game();

  REQUIRE(no_shield_score == 0);
  REQUIRE(shielded_score == SHIELD_BONUS);
}

TEST_CASE("KingSafetyEvaluator penalizes attackers reaching into the king's "
          "zone",
          "[eval][king_safety]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  KingSafetyEvaluator eval;

  BughousePosition pos;
  // Black queen on e5 attacks down the e-file into White's king zone
  // Hits e2 and (being blocked by the king) e1 -- 2 squares.
  pos.boards[0].load_fen("4k3/8/8/4q3/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  int score = eval.evaluate(to_context(pos, search)).mid_game();
  REQUIRE(score == -(2 * ATTACK_UNIT_PENALTY));
}

TEST_CASE("KingSafetyEvaluator adds a pocket-threat bonus proportional to the "
          "piece's value and pocket bonus",
          "[eval][king_safety]") {
  BughouseClock clock = make_clock();
  KingSafetyEvaluator eval;

  auto score_with_pocket_piece = [&](PieceType pt) {
    BughousePosition pos;
    pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
    pos.pockets[0].add(pt); // player 0's pocket threatens the Black king
    SearchContext search = make_context(clock, to_player(0));
    return eval.evaluate(to_context(pos, search)).mid_game();
  };

  REQUIRE(score_with_pocket_piece(QUEEN) ==
          (SEE::PIECE_VALUE[QUEEN] + SEE::POCKET_BONUS[QUEEN]) / 100);
  REQUIRE(score_with_pocket_piece(ROOK) ==
          (SEE::PIECE_VALUE[ROOK] + SEE::POCKET_BONUS[ROOK]) / 100);
  REQUIRE(score_with_pocket_piece(BISHOP) ==
          (SEE::PIECE_VALUE[BISHOP] + SEE::POCKET_BONUS[BISHOP]) / 100);
  REQUIRE(score_with_pocket_piece(KNIGHT) ==
          (SEE::PIECE_VALUE[KNIGHT] + SEE::POCKET_BONUS[KNIGHT]) / 100);
}

TEST_CASE("KingSafetyEvaluator ignores pawns in the pocket-threat bonus",
          "[eval][king_safety]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.pockets[0].add(PAWN);
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  KingSafetyEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("KingSafetyEvaluator is antisymmetric between opposing players",
          "[eval][king_safety]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/4q3/8/8/4P3/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  KingSafetyEvaluator eval;

  int score0 = eval.evaluate(to_context(pos, make_context(clock, to_player(0))))
                   .mid_game();
  int score1 = eval.evaluate(to_context(pos, make_context(clock, to_player(1))))
                   .mid_game();

  REQUIRE(score0 == -score1);
}