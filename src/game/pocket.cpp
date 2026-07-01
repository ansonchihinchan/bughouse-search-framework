#include "game/pocket.h"
#include <iostream>

bool Pocket::empty() const {
  for (int pt = PAWN; pt <= QUEEN; pt++)
    if (pockets[pt] > 0)
      return false;
  return true;
}

void Pocket::print() const {
  const char *names[] = {"", "P", "N", "B", "R", "Q"};
  bool any = false;
  for (int pt = PAWN; pt <= QUEEN; pt++) {
    for (int i = 0; i < pockets[pt]; i++) {
      std::cout << names[pt];
      any = true;
    }
  }
  if (!any)
    std::cout << "(empty)";
  std::cout << '\n';
}