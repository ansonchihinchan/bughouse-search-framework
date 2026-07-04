#include <catch2/catch_all.hpp>

#include "game/board.h"

namespace {
constexpr const char *START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

TEST_CASE("Default-constructed board is the standard start position",
          "[board][fen]") {
  Board b;
  REQUIRE(b.to_fen() == START_FEN);
  REQUIRE(b.sideToMove == WHITE);
  REQUIRE(b.castlingRights == ANY_CASTLING);
  REQUIRE(b.enPassantSquare == -1);
  REQUIRE(b.halfMove == 0);
  REQUIRE(b.fullMove == 1);
  REQUIRE_FALSE(b.is_in_check());
  REQUIRE_FALSE(b.is_checkmate());
  REQUIRE_FALSE(b.is_stalemate());
}

TEST_CASE("load_fen round-trips an arbitrary legal position", "[board][fen]") {
  Board b;
  std::string fen = "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w "
                    "KQkq - 2 3";
  REQUIRE(b.load_fen(fen));
  REQUIRE(b.to_fen() == fen);
}

TEST_CASE("load_fen rejects malformed FEN strings", "[board][fen]") {
  Board b;

  SECTION("garbage input") { REQUIRE_FALSE(b.load_fen("not a fen at all")); }

  SECTION("missing a king") {
    // Only a white king present -- invalid, must be rejected.
    REQUIRE_FALSE(b.load_fen("4r3/8/8/8/8/8/8/4K3 w - - 0 1"));
  }

  SECTION("bad side-to-move token") {
    REQUIRE_FALSE(
        b.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1"));
  }

  SECTION("bad castling token") {
    REQUIRE_FALSE(
        b.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w ZZZZ - 0 1"));
  }

  SECTION("bad en passant square") {
    REQUIRE_FALSE(b.load_fen(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq z9 0 1"));
  }
}

TEST_CASE("load_fen leaves the board untouched on failure", "[board][fen]") {
  Board b(START_FEN);
  std::string before = b.to_fen();

  REQUIRE_FALSE(b.load_fen("garbage"));
  REQUIRE(b.to_fen() == before);
}

TEST_CASE("make_move/undo_move restores FEN and hash exactly", "[board]") {
  Board b;
  std::string before_fen = b.to_fen();
  uint64_t before_hash = b.hash;

  Move e4 = Move::normal(12, 28); // e2e4
  UndoInfo undo = b.make_move(e4);

  REQUIRE(b.to_fen() != before_fen);
  REQUIRE(b.sideToMove == BLACK);

  b.undo_move(e4, undo);

  REQUIRE(b.to_fen() == before_fen);
  REQUIRE(b.hash == before_hash);
  REQUIRE(b.sideToMove == WHITE);
}

TEST_CASE("Double pawn push sets the en passant square", "[board]") {
  Board b;
  Move e4 = Move::normal(12, 28); // e2e4
  b.make_move(e4);

  REQUIRE(b.enPassantSquare == to_square(4, 2)); // e3
}

TEST_CASE("Single pawn push does not set an en passant square", "[board]") {
  Board b;
  Move e3 = Move::normal(12, 20); // e2e3
  b.make_move(e3);

  REQUIRE(b.enPassantSquare == -1);
}

TEST_CASE("Promotion replaces the pawn with the chosen piece", "[board]") {
  // White pawn one step from promoting; both kings present so the FEN is
  // valid.
  Board b("7k/P7/8/8/8/8/8/7K w - - 0 1");

  Move promo = Move::promote(to_square(0, 6), to_square(0, 7), QUEEN);
  b.make_move(promo);

  REQUIRE(b.piece_on(to_square(0, 7)) == make_piece(WHITE, QUEEN));
  REQUIRE(b.is_empty(to_square(0, 6)));
}

TEST_CASE("Promotion undo restores the original pawn", "[board]") {
  Board b("7k/P7/8/8/8/8/8/7K w - - 0 1");
  Move promo = Move::promote(to_square(0, 6), to_square(0, 7), QUEEN);

  UndoInfo undo = b.make_move(promo);
  b.undo_move(promo, undo);

  REQUIRE(b.piece_on(to_square(0, 6)) == make_piece(WHITE, PAWN));
  REQUIRE(b.is_empty(to_square(0, 7)));
}

TEST_CASE("Promotion that also captures restores both pieces on undo",
          "[board]") {
  // White pawn on b7 can promote by capturing the rook on a8.
  Board b("r6k/1P6/8/8/8/8/8/7K w - - 0 1");
  std::string before = b.to_fen();

  Move promoCapture =
      Move::promote(to_square(1, 6), to_square(0, 7), QUEEN); // b7xa8=Q
  UndoInfo undo = b.make_move(promoCapture);

  REQUIRE(b.piece_on(to_square(0, 7)) == make_piece(WHITE, QUEEN));

  b.undo_move(promoCapture, undo);

  REQUIRE(b.to_fen() == before);
  REQUIRE(b.piece_on(to_square(1, 6)) == make_piece(WHITE, PAWN)); // b7
  REQUIRE(b.piece_on(to_square(0, 7)) == make_piece(BLACK, ROOK)); // a8
}

TEST_CASE("Kingside castling moves both king and rook", "[board]") {
  Board b("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  Move castle = Move::castling(to_square(4, 0), to_square(6, 0)); // e1g1

  b.make_move(castle);

  REQUIRE(b.piece_on(to_square(6, 0)) == make_piece(WHITE, KING)); // g1
  REQUIRE(b.piece_on(to_square(5, 0)) == make_piece(WHITE, ROOK)); // f1
  REQUIRE(b.is_empty(to_square(4, 0)));                            // e1
  REQUIRE(b.is_empty(to_square(7, 0)));                            // h1
  // Moving the king forfeits both white castling rights.
  REQUIRE((b.castlingRights & WHITE_CASTLING) == NO_CASTLING);
  REQUIRE((b.castlingRights & BLACK_CASTLING) == BLACK_CASTLING);
}

TEST_CASE("Castling undo restores king, rook, and castling rights", "[board]") {
  Board b("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  CastlingRights before_rights = b.castlingRights;
  Move castle = Move::castling(to_square(4, 0), to_square(6, 0));

  UndoInfo undo = b.make_move(castle);
  b.undo_move(castle, undo);

  REQUIRE(b.piece_on(to_square(4, 0)) == make_piece(WHITE, KING));
  REQUIRE(b.piece_on(to_square(7, 0)) == make_piece(WHITE, ROOK));
  REQUIRE(b.castlingRights == before_rights);
}

TEST_CASE("Moving a rook off its home square forfeits only that side",
          "[board]") {
  Board b("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  Move rookMove = Move::normal(to_square(0, 0), to_square(0, 1)); // a1a2

  b.make_move(rookMove);

  REQUIRE((b.castlingRights & WHITE_OOO) == NO_CASTLING); // lost queenside
  REQUIRE((b.castlingRights & WHITE_OO) == WHITE_OO);     // kept kingside
  REQUIRE((b.castlingRights & BLACK_CASTLING) == BLACK_CASTLING);
}

TEST_CASE("En passant capture removes the passed pawn", "[board]") {
  // White pawn on e5, black just double-pushed d7-d5, en passant available.
  Board b("7k/8/8/3pP3/8/8/8/7K w - d6 0 2");
  Move ep = Move::en_passant(to_square(4, 4), to_square(3, 5)); // e5xd6

  UndoInfo undo = b.make_move(ep);

  REQUIRE(b.piece_on(to_square(3, 5)) == make_piece(WHITE, PAWN));
  REQUIRE(b.is_empty(to_square(3, 4))); // captured black pawn removed
  REQUIRE(b.is_empty(to_square(4, 4)));

  b.undo_move(ep, undo);
  REQUIRE(b.piece_on(to_square(4, 4)) == make_piece(WHITE, PAWN));
  REQUIRE(b.piece_on(to_square(3, 4)) == make_piece(BLACK, PAWN));
  REQUIRE(b.is_empty(to_square(3, 5)));
}

TEST_CASE("is_attacked reports squares covered by a rook on an open file",
          "[board]") {
  Board b("k3r3/8/8/8/8/8/8/4K3 w - - 0 1");

  REQUIRE(b.is_attacked(to_square(4, 0), BLACK));       // e1 attacked by Re8
  REQUIRE_FALSE(b.is_attacked(to_square(3, 0), BLACK)); // d1 is safe
}

TEST_CASE("is_checkmate recognizes Fool's Mate", "[board][mate]") {
  // Verified against this engine's own move generator:
  // 1. f3 e5 2. g4 Qh4#
  Board b("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 2 3");

  REQUIRE(b.is_in_check());
  REQUIRE(b.is_checkmate());
  REQUIRE_FALSE(b.is_stalemate());
}

TEST_CASE("is_stalemate recognizes a classic stalemate position",
          "[board][mate]") {
  Board b("k7/8/1Q6/8/8/8/8/7K b - - 0 1");

  REQUIRE_FALSE(b.is_in_check());
  REQUIRE(b.is_stalemate());
  REQUIRE_FALSE(b.is_checkmate());
}

TEST_CASE("is_capture identifies captures including en passant", "[board]") {
  Board start;
  Move e4 = Move::normal(12, 28);
  REQUIRE_FALSE(start.is_capture(e4)); // moving to an empty square

  Board b("7k/8/8/3pP3/8/8/8/7K w - d6 0 2");
  Move ep = Move::en_passant(to_square(4, 4), to_square(3, 5));
  REQUIRE(b.is_capture(ep));
}