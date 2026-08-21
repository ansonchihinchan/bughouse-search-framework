#include <catch2/catch_all.hpp>

#include "communication/generator.h"

namespace {
constexpr PlayerId ROOT = PlayerId{0};

BughousePosition open_kings() {
  BughousePosition position;
  position.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  return position;
}

SearchResult result_with_score(int score) {
  SearchResult result;
  result.score = score;
  return result;
}

Message generate(BughousePosition position, int score = 0) {
  return Generator{}.generate_message(result_with_score(score), position, ROOT);
}

void add(Pocket &pocket, PieceType piece, int count) {
  for (int i = 0; i < count; i++)
    pocket.add(piece);
}

void suppress_other_requests(Pocket &pocket) {
  add(pocket, PAWN, 20);
  add(pocket, ROOK, 20);
  add(pocket, QUEEN, 20);
}
} // namespace

TEST_CASE("generator creates a bounded checking-piece request",
          "[communication][generator][piece]") {
  Message message = generate(open_kings());

  REQUIRE(message.piece_request.piece != NO_PIECE_TYPE);
  REQUIRE(message.piece_request.confidence >= 0.0f);
  REQUIRE(message.piece_request.confidence <= 1.0f);
  REQUIRE(message.piece_request.eta_plies > 0);
}

TEST_CASE("piece request marginal value falls with pocket multiplicity",
          "[communication][generator][piece][multiplicity]") {
  BughousePosition knight_abundant = open_kings();
  suppress_other_requests(knight_abundant.pockets[0]);
  add(knight_abundant.pockets[0], KNIGHT, 3);
  REQUIRE(generate(knight_abundant).piece_request.piece == BISHOP);

  BughousePosition bishop_abundant = open_kings();
  suppress_other_requests(bishop_abundant.pockets[0]);
  add(bishop_abundant.pockets[0], BISHOP, 3);
  REQUIRE(generate(bishop_abundant).piece_request.piece == KNIGHT);

  BughousePosition equal = open_kings();
  suppress_other_requests(equal.pockets[0]);
  add(equal.pockets[0], KNIGHT, 2);
  add(equal.pockets[0], BISHOP, 2);
  REQUIRE(generate(equal).piece_request.piece == BISHOP);

  BughousePosition mixed = equal;
  add(mixed.pockets[0], BISHOP, 3);
  REQUIRE(generate(mixed).piece_request.piece == KNIGHT);
}

TEST_CASE("available requested pieces have shorter ETA than unavailable ones",
          "[communication][generator][eta]") {
  BughousePosition missing = open_kings();
  suppress_other_requests(missing.pockets[0]);
  add(missing.pockets[0], BISHOP, 20);
  int missing_eta = generate(missing).piece_request.eta_plies;

  BughousePosition available = missing;
  available.pockets[0].add(KNIGHT);
  int available_eta = generate(available).piece_request.eta_plies;

  REQUIRE(generate(missing).piece_request.piece == KNIGHT);
  REQUIRE(generate(available).piece_request.piece == KNIGHT);
  REQUIRE(available_eta < missing_eta);
}

TEST_CASE("partner capture opportunity shortens piece request ETA",
          "[communication][generator][eta]") {
  BughousePosition unavailable = open_kings();
  suppress_other_requests(unavailable.pockets[0]);
  add(unavailable.pockets[0], BISHOP, 20);

  BughousePosition capturable = unavailable;
  capturable.boards[1].load_fen("4k3/8/8/8/8/8/4N3/4r1K1 b - - 0 1");

  REQUIRE(generate(capturable).piece_request.piece == KNIGHT);
  REQUIRE(generate(capturable).piece_request.eta_plies <
          generate(unavailable).piece_request.eta_plies);
}

TEST_CASE("generator omits a piece request with no useful drop square",
          "[communication][generator][piece]") {
  BughousePosition position = open_kings();
  position.boards[0].load_fen(
      "nnnnknnn/nnnnnnnn/nnnnnnnn/nnnnnnnn/NNNNNNNN/NNNNNNNN/NNNNNNNN/"
      "NNNNKNNN w - - 0 1");

  PieceRequest request = generate(position).piece_request;
  REQUIRE(request.piece == NO_PIECE_TYPE);
  REQUIRE(request.confidence == 0.0f);
}

TEST_CASE("generator produces every supported strategic signal",
          "[communication][generator][strategy]") {
  BughousePosition quiet = open_kings();
  REQUIRE(generate(quiet, 0).strat_request.strat == StrategyType::Stall);
  REQUIRE(generate(quiet, -500).strat_request.strat ==
          StrategyType::AvoidTrades);
  REQUIRE(generate(quiet, 500).strat_request.strat ==
          StrategyType::TradeEverything);
  REQUIRE(generate(quiet, 800).strat_request.strat == StrategyType::AttackNow);

  BughousePosition defended = quiet;
  defended.boards[0].load_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
  REQUIRE(generate(defended).strat_request.strat == StrategyType::Defend);

  std::array<int64_t, PLAYER_NO> remaining{60'000, 60'000, 30'000, 5'000};
  Message flag = Generator{}.generate_message(result_with_score(0), quiet, ROOT,
                                              remaining);
  REQUIRE(flag.strat_request.strat == StrategyType::Flag);
}

TEST_CASE("forcing safety and attack signals precede clock strategy",
          "[communication][generator][strategy][precedence]") {
  std::array<int64_t, PLAYER_NO> remaining{60'000, 60'000, 30'000, 5'000};

  BughousePosition attack = open_kings();
  attack.pockets[0].add(KNIGHT);
  Message attacking = Generator{}.generate_message(result_with_score(0), attack,
                                                   ROOT, remaining);
  REQUIRE(attacking.strat_request.strat == StrategyType::AttackNow);

  BughousePosition defence = attack;
  defence.boards[0].load_fen("4k3/8/8/8/8/8/4r3/4K3 w - - 0 1");
  Message defending = Generator{}.generate_message(result_with_score(0),
                                                   defence, ROOT, remaining);
  REQUIRE(defending.strat_request.strat == StrategyType::Defend);
}

TEST_CASE("strategy confidence is bounded and strengthens with evidence",
          "[communication][generator][strategy][confidence]") {
  BughousePosition position = open_kings();
  StrategyRequest weak = generate(position, 600).strat_request;
  StrategyRequest strong = generate(position, 1600).strat_request;

  REQUIRE(weak.strat == StrategyType::AttackNow);
  REQUIRE(strong.strat == StrategyType::AttackNow);
  REQUIRE(weak.confidence >= 0.0f);
  REQUIRE(strong.confidence <= 1.0f);
  REQUIRE(strong.confidence > weak.confidence);
}

TEST_CASE("generator output is deterministic and identifies the sender",
          "[communication][generator]") {
  BughousePosition position = open_kings();
  position.boards[0].fullMove = 17;
  SearchResult search = result_with_score(123);

  Message first = Generator{}.generate_message(search, position, ROOT);
  Message second = Generator{}.generate_message(search, position, ROOT);

  REQUIRE(first.sender == ROOT);
  REQUIRE(first.move_no == 17);
  REQUIRE(first.piece_request.piece == second.piece_request.piece);
  REQUIRE(first.piece_request.confidence == second.piece_request.confidence);
  REQUIRE(first.piece_request.eta_plies == second.piece_request.eta_plies);
  REQUIRE(first.strat_request.strat == second.strat_request.strat);
  REQUIRE(first.strat_request.confidence == second.strat_request.confidence);
}