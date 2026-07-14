#include "positions.h"

#include "game/movegen.h"
#include "game/types.h"
#include <stdexcept>

namespace {

Square parse_square(const std::string &s) {
  int file = s[0] - 'a';
  int rank = s[1] - '1';
  return to_square(file, rank);
}

// Replays a list of UCI move tokens against `board`, resolving each one to
// the matching legal Move via the engine's own move generator. Throws if a
// token doesn't correspond to a legal move -- i.e. this is a self-check that
// every position in the suite below is reachable by legal play.
void apply_uci_moves(Board &board, const std::vector<std::string> &moves) {
  for (const std::string &token : moves) {
    if (token.size() < 4)
      throw std::runtime_error("bench: malformed move token '" + token + "'");

    Square from = parse_square(token.substr(0, 2));
    Square to = parse_square(token.substr(2, 2));

    PieceType promo = NO_PIECE_TYPE;
    if (token.size() == 5) {
      static const std::string pts = " pnbrqk";
      size_t idx = pts.find(token[4]);
      if (idx == std::string::npos)
        throw std::runtime_error("bench: bad promotion in '" + token + "'");
      promo = static_cast<PieceType>(idx);
    }

    std::vector<Move> candidates = generate_pseudo_legal_moves(board);
    const Move *chosen = nullptr;
    for (const Move &m : candidates) {
      if (m.from == from && m.to == to &&
          (token.size() < 5 || m.promote_pt == promo)) {
        chosen = &m;
        break;
      }
    }

    if (!chosen || !board.is_legal(*chosen))
      throw std::runtime_error("bench: '" + token +
                               "' is not a legal move in this position; "
                               "fix the benchmark suite in bench/positions.cpp");

    board.make_move(*chosen);
  }
}

Board build_board(const std::vector<std::string> &moves,
                  const std::string &fen_override) {
  if (!fen_override.empty())
    return Board(fen_override);

  Board board;
  board.reset();
  apply_uci_moves(board, moves);
  return board;
}

Pocket build_pocket(const std::string &letters) {
  Pocket pocket;
  static const std::string pts = " PNBRQK";
  for (char c : letters) {
    size_t idx = pts.find(c);
    if (idx == std::string::npos || idx == KING)
      throw std::runtime_error(std::string("bench: bad pocket letter '") + c +
                               "'");
    pocket.add(static_cast<PieceType>(idx));
  }
  return pocket;
}

} // namespace

BughousePosition build_position(const BenchPosition &spec) {
  BughousePosition position;
  position.boards[0] = build_board(spec.board0_moves, spec.board0_fen);
  position.boards[1] = build_board(spec.board1_moves, spec.board1_fen);

  for (int p = 0; p < PLAYER_NO; p++)
    position.pockets[p] = build_pocket(spec.pockets[p]);

  return position;
}

const std::vector<BenchPosition> &benchmark_suite() {
  static const std::vector<BenchPosition> suite = {
      {
          .name = "start",
          .description = "Standard bughouse start position on both boards, "
                         "empty pockets. Baseline opening branching factor.",
          .board0_moves = {},
          .board1_moves = {},
          .pockets = {"", "", "", ""},
          .root_player = 0,
      },
      {
          .name = "two_openings",
          .description = "Board 0 in a Ruy Lopez (Morphy Defense), board 1 "
                         "in a Queen's Gambit Declined. Representative "
                         "opening/early-middlegame branching, no pockets.",
          .board0_moves = {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6",
                           "b5a4", "g8f6"},
          .board1_moves = {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6"},
          .pockets = {"", "", "", ""},
          .root_player = 0,
      },
      {
          .name = "heavy_pockets",
          .description = "Same boards as two_openings, but every player has "
                         "a large reserve. Stresses drop-move generation and "
                         "the resulting blow-up in branching factor, which "
                         "is the main bughouse-specific search cost.",
          .board0_moves = {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6",
                           "b5a4", "g8f6"},
          .board1_moves = {"d2d4", "d7d5", "c2c4", "e7e6", "b1c3", "g8f6"},
          .pockets = {"PPPNB", "PPR", "PPPQ", "PPNN"},
          .root_player = 0,
      },
      {
          .name = "fried_liver_tactics",
          .description = "Fried Liver Attack up to 7.Nxf7 -- a sharp, "
                         "capture-rich tactical position exercising capture "
                         "ordering and quiescence search.",
          .board0_moves = {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4", "g8f6",
                           "f3g5", "d7d5", "e4d5", "f6d5", "g5f7"},
          .board1_moves = {},
          .pockets = {"", "", "", ""},
          .root_player = 1, // black to move on board 0 after 7. Nxf7
      },
      {
          .name = "sparse_endgame",
          .description = "Sparse king-and-pawn / king-and-rook endgames on "
                         "both boards. Low branching factor; useful for "
                         "seeing EBF and NPS away from the opening.",
          .board0_fen = "8/5k2/8/8/8/3K4/4P3/8 w - - 0 1",
          .board1_fen = "8/8/4k3/8/8/2KR4/8/8 w - - 0 1",
          .pockets = {"", "", "", ""},
          .root_player = 0,
      },
      {
          .name = "check_evasion",
          .description = "White king in check on board 0 from a lone black "
                         "rook. Exercises check-evasion move generation and "
                         "the in-check branch of quiescence search.",
          .board0_fen = "4k3/8/8/8/8/8/4r3/4K3 w - - 0 1",
          .board1_fen = "",
          .pockets = {"", "", "", ""},
          .root_player = 0,
      },
  };
  return suite;
}
