#include <catch2/catch_all.hpp>

#include "game/attacks.h"
#include "game/movegen.h"
#include "game/pocket.h"
#include <algorithm>
#include <array>

namespace {
bool contains_move(const std::vector<Move> &moves, Move m) {
  return std::find(moves.begin(), moves.end(), m) != moves.end();
}

} // namespace

TEST_CASE("generate_pseudo_legal_moves produces the standard 20 opening moves",
          "[movegen]") {
  Board b;
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE(moves.size() == 20);
}

TEST_CASE("generate_pseudo_legal_moves includes single and double pawn pushes",
          "[movegen][pawn]") {
  Board b;
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 3))));
}

TEST_CASE("generate_pseudo_legal_moves excludes double push when blocked",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/4n3/8/4P3/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 3))));
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
}

TEST_CASE("generate_pseudo_legal_moves excludes single push when the target is "
          "occupied",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/8/4n3/4P3/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
}

TEST_CASE("generate_pseudo_legal_moves gives pawns diagonal captures only onto "
          "enemy pieces",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/8/3p1p2/4P3/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(3, 2))));
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(5, 2))));
}

TEST_CASE(
    "generate_pseudo_legal_moves does not let pawns capture straight ahead",
    "[movegen][pawn]") {
  Board b("k7/8/8/8/8/4p3/4P3/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
}

TEST_CASE(
    "generate_pseudo_legal_moves produces all four promotion piece choices",
    "[movegen][promotion]") {
  Board b("7k/P7/8/8/8/8/8/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  Square from = to_square(0, 6);
  Square to = to_square(0, 7);
  REQUIRE(contains_move(moves, Move::promote(from, to, QUEEN)));
  REQUIRE(contains_move(moves, Move::promote(from, to, ROOK)));
  REQUIRE(contains_move(moves, Move::promote(from, to, BISHOP)));
  REQUIRE(contains_move(moves, Move::promote(from, to, KNIGHT)));
}

TEST_CASE("generate_pseudo_legal_moves includes en passant capture immediately "
          "after a "
          "double push",
          "[movegen][enpassant]") {
  Board b("7k/8/8/3pP3/8/8/8/7K w - d6 0 2");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE(
      contains_move(moves, Move::en_passant(to_square(4, 4), to_square(3, 5))));
}

TEST_CASE("knight move generation ignores blocking pieces",
          "[movegen][knight]") {
  Board b("k7/8/8/2PPP3/2PNP3/2PPP3/8/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  int from = to_square(3, 3);
  REQUIRE(contains_move(moves, Move::normal(from, to_square(4, 5))));
  REQUIRE(contains_move(moves, Move::normal(from, to_square(2, 5))));
}

TEST_CASE("sliding pieces stop at the first blocker and cannot jump over it",
          "[movegen][sliding]") {
  Board b("k7/8/8/8/3p4/8/3R4/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  int from = to_square(3, 1);
  // Rook can capture the pawn on d4
  REQUIRE(contains_move(moves, Move::normal(from, to_square(3, 3))));
  // Rook cannot continue past d5
  REQUIRE_FALSE(contains_move(moves, Move::normal(from, to_square(3, 4))));
}

TEST_CASE("sliding pieces cannot capture a friendly piece",
          "[movegen][sliding]") {
  Board b("k7/8/8/8/3P4/8/3R4/7K w - - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  int from = to_square(3, 1);
  REQUIRE_FALSE(contains_move(moves, Move::normal(from, to_square(3, 3))));
  REQUIRE_FALSE(contains_move(moves, Move::normal(from, to_square(3, 4))));
}

TEST_CASE("generate_pseudo_legal_moves includes kingside and queenside "
          "castling when legal",
          "[movegen][castling]") {
  Board b("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
  REQUIRE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(2, 0))));
}

TEST_CASE(
    "generate_pseudo_legal_moves excludes castling through an attacked square",
    "[movegen][castling]") {
  // Black rook on f8 attacks f1
  Board b("r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
}

TEST_CASE("generate_pseudo_legal_moves excludes castling while in check",
          "[movegen][castling]") {
  Board b("r3k2r/8/8/8/8/8/4r3/R3K2R w KQkq - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(2, 0))));
}

TEST_CASE("generate_pseudo_legal_moves excludes castling when squares between "
          "king and "
          "rook are occupied",
          "[movegen][castling]") {
  Board b("r3k2r/8/8/8/8/8/8/RN2K1NR w KQkq - 0 1");
  auto moves = generate_pseudo_legal_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(2, 0))));
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
}

TEST_CASE("generate_drops produces no drops for an empty pocket",
          "[movegen][drops]") {
  Board b;
  Pocket p;
  auto drops = generate_drop_moves(b, p);
  REQUIRE(drops.empty());
}

TEST_CASE("generate_drops targets every empty square for a held piece type",
          "[movegen][drops]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Pocket p;
  p.add(KNIGHT);
  auto drops = generate_drop_moves(b, p);
  // 64 squares minus the 2 occupied by kings.
  REQUIRE(drops.size() == 62);
}

TEST_CASE("generate_drops excludes rank 1 and rank 8 for pawn drops",
          "[movegen][drops]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Pocket p;
  p.add(PAWN);
  auto drops = generate_drop_moves(b, p);
  for (auto &m : drops) {
    REQUIRE(rank_of(m.to) != 0);
    REQUIRE(rank_of(m.to) != 7);
  }
  // 64 squares - 16 (two back ranks)
  REQUIRE(drops.size() == 48);
}

TEST_CASE("generate_pseudo_legal_moves with a pocket appends legal drop moves",
          "[movegen][drops]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Pocket p;
  p.add(QUEEN);
  auto moves = generate_pseudo_legal_moves(b, &p);
  REQUIRE(contains_move(moves, Move::drop(QUEEN, to_square(4, 3))));
}

TEST_CASE("drop_check_squares exactly matches generated checking drops",
          "[movegen][drops][attacks]") {
  const std::array<std::string, 4> fens{
      "r1bqk2r/1ppp1ppp/p1n2n2/4p3/B3P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 2 5",
      "4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1",
      "4k3/pppppppp/8/8/8/8/PPPPPPPP/4K3 w - - 0 1",
      "8/8/8/3k4/8/4K3/8/8 w - - 0 1"};

  for (const std::string &fen : fens) {
    Board board(fen);
    for (Colour mover : {WHITE, BLACK}) {
      for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
        Pocket pocket;
        pocket.add(pt);
        Bitboard generated = 0;
        for (const Move &move : generate_drop_moves(board, pocket)) {
          if (drop_gives_check(board, pt, move.to, mover))
            generated |= 1ULL << move.to;
        }
        REQUIRE(drop_check_squares(board, pt, mover) == generated);
      }
    }
  }
}

TEST_CASE("perft(0) always returns 1 regardless of position",
          "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 0) == 1);
}

TEST_CASE("perft(1) from the start position equals 20", "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 1) == 20);
}

TEST_CASE("perft(2) from the start position equals 400", "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 2) == 400);
}

TEST_CASE("perft(3) from the start position equals 8902", "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 3) == 8902);
}

TEST_CASE("perft(4) from the start position equals 197281",
          "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 4) == 197281);
}

TEST_CASE("Kiwipete perft exercises castling pins and captures",
          "[movegen][perft][oracle]") {
  // Canonical position 2 from the public perft suite also used by Stockfish
  Board b("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/"
          "R3K2R w KQkq - 0 1");
  REQUIRE(perft(b, 1) == 48);
  REQUIRE(perft(b, 2) == 2039);
  const std::array<std::pair<const char *, uint64_t>, 48> divide{
      {{"a1b1", 1969}, {"a1c1", 1968}, {"a1d1", 1885}, {"a2a3", 2186},
       {"a2a4", 2149}, {"b2b3", 1964}, {"c3a4", 2203}, {"c3b1", 2038},
       {"c3b5", 2138}, {"c3d1", 2040}, {"d2c1", 1963}, {"d2e3", 2136},
       {"d2f4", 2000}, {"d2g5", 2134}, {"d2h6", 2019}, {"d5d6", 1991},
       {"d5e6", 2241}, {"e1c1", 1887}, {"e1d1", 1894}, {"e1f1", 1855},
       {"e1g1", 2059}, {"e2a6", 1907}, {"e2b5", 2057}, {"e2c4", 2082},
       {"e2d1", 1733}, {"e2d3", 2050}, {"e2f1", 2060}, {"e5c4", 1880},
       {"e5c6", 2027}, {"e5d3", 1803}, {"e5d7", 2124}, {"e5f7", 2080},
       {"e5g4", 1878}, {"e5g6", 1997}, {"f3d3", 2005}, {"f3e3", 2174},
       {"f3f4", 2132}, {"f3f5", 2396}, {"f3f6", 2111}, {"f3g3", 2214},
       {"f3g4", 2169}, {"f3h3", 2360}, {"f3h5", 2267}, {"g2g3", 1882},
       {"g2g4", 1843}, {"g2h3", 1970}, {"h1f1", 1929}, {"h1g1", 2013}}};
  const std::vector<Move> moves = generate_pseudo_legal_moves(b);
  for (const auto &[notation, expected] : divide) {
    const auto move =
        std::find_if(moves.begin(), moves.end(), [&](Move candidate) {
          return candidate.to_string() == notation;
        });
    REQUIRE(move != moves.end());
    BoardUndo undo = b.make_move(*move);
    const uint64_t actual = perft(b, 2);
    b.undo_move(*move, undo);
    INFO("divide move " << notation);
    REQUIRE(actual == expected);
  }
}

TEST_CASE("canonical perft corpus covers tactical rule edge cases",
          "[movegen][perft][oracle]") {
  struct PerftCase {
    const char *fen;
    std::array<uint64_t, 3> nodes;
  };
  const std::array<PerftCase, 4> corpus{{
      {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {14, 191, 2812}},
      {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
       {6, 264, 9467}},
      {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
       {44, 1486, 62379}},
      {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/"
       "R4RK1 w - - 0 10",
       {46, 2079, 89890}},
  }};

  for (const PerftCase &entry : corpus) {
    Board board(entry.fen);
    INFO("FEN: " << entry.fen);
    for (int depth = 1; depth <= 3; ++depth)
      REQUIRE(perft(board, depth) == entry.nodes[depth - 1]);
  }
}