#include "game/bughouse.h"
#include "game/movegen.h"
#include <iostream>
#include <sstream>
#include <string>

static Square parse_square(const std::string &square) {
  if (square.size() < 2)
    return -1;
  int file = square[0] - 'a';
  int rank = square[1] - '1';
  if (file < 0 || file > 7 || rank < 0 || rank > 7)
    return -1;
  return to_square(file, rank);
}

static Move parse_move(const std::string &token) {
  // e.g. "N@e4"
  if (token.size() >= 3 && token[1] == '@') {
    const std::string pts = "PNBRQK";
    size_t idx = pts.find(token[0]);
    if (idx == std::string::npos)
      return Move{};
    Square to = parse_square(token.substr(2));
    if (to == -1)
      return Move{};
    return Move::drop(static_cast<PieceType>(idx + 1), to);
  }

  if (token.size() < 4)
    return Move{};

  // e.g. "e2e4"
  Square from = parse_square(token.substr(0, 2));
  Square to = parse_square(token.substr(2, 2));

  if (from == -1 || to == -1)
    return Move{};

  // e.g. "e7e8q"
  if (token.size() == 5) {
    const std::string pts = " pnbrqk";
    PieceType promo = static_cast<PieceType>(pts.find(token[4]));
    return Move::promote(from, to, promo);
  }

  return Move::normal(from, to);
}

int main() {
  Board::init_zobrist();

  BughouseState game;
  game.print();

  std::cout << "Commands: 'A <move>' for board A, 'B <move>' for board B\n";
  std::cout << "          'test <depth>' to test move gen, 'quit' to exit\n\n";
  ;

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "quit")
      break;

    std::istringstream ss(line);
    std::string cmd;
    ss >> cmd;

    if (cmd == "test") {
      int depth = 1;
      ss >> depth;
      Board board;
      board.reset();
      uint64_t nodes = perft(board, depth);
      std::cout << "perft(" << depth << ") = " << nodes << '\n';
      continue;
    }

    int board_idx = -1;
    std::string move_str = cmd;
    if (cmd == "A") {
      board_idx = 0;
    } else if (cmd == "B") {
      board_idx = 1;
    } else {
      std::cout << "Unknown command. Try 'A <move>', 'B <move>', 'test "
                   "<depth>', 'quit'\n";
      continue;
    }

    if (!(ss >> move_str)) {
      std::cout << "Usage: " << cmd << " <move>  (e.g. " << cmd << " e2e4)\n";
      continue;
    }

    Move move = parse_move(move_str);
    if (move.is_none()) {
      std::cout << "Could not parse move '" << move_str << "'\n";
      continue;
    }

    Colour stm = game.boards[board_idx].sideToMove;
    int player_id = board_idx == 0 ? stm : 3 - stm;

    if (!game.apply_move(player_id, move)) {
      std::cout << "Illegal move '" << move_str << "' on board " << cmd << '\n';
      continue;
    }

    game.print();

    switch (game.result()) {
    case GameResult::TEAM_A_WINS:
      std::cout << "Team A wins.\n";
      break;
    case GameResult::TEAM_B_WINS:
      std::cout << "Team B wins.\n";
      break;
    case GameResult::DRAW:
      std::cout << "Draw.\n";
      break;
    default:
      continue;
    }
  }

  return 0;
}