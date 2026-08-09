#include <catch2/catch_all.hpp>

#include "game/clock.h"
#include <thread>

TEST_CASE("BughouseClock::set initialises all players and clears active player",
          "[clock]") {
  BughouseClock c;
  c.set(5000, 250);

  for (int i = 0; i < 4; i++)
    REQUIRE(c.time_ms[i] == 5000);
  REQUIRE(c.increment_ms == 250);
  REQUIRE(c.active_player == -1);
}

TEST_CASE("remaining() returns the stored time when no player is active",
          "[clock]") {
  BughouseClock c;
  c.set(1000, 100);

  for (int i = 0; i < 4; i++)
    REQUIRE(c.remaining(to_player(i)) == 1000);
}

TEST_CASE("remaining() counts down for the active player only", "[clock]") {
  BughouseClock c;
  c.set(10000, 0);
  c.start(to_player(0));

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  int64_t active_remaining = c.remaining(to_player(0));
  int64_t inactive_remaining = c.remaining(to_player(1));

  REQUIRE(active_remaining < 10000);
  REQUIRE(active_remaining > 10000 - 500); // generous tolerance
  REQUIRE(inactive_remaining == 10000);    // untouched, not active
}

TEST_CASE("stop() banks elapsed time and adds the increment", "[clock]") {
  BughouseClock c;
  c.set(1000, 100);
  c.start(to_player(0));
  c.stop(to_player(0));

  REQUIRE(c.active_player == -1);
  REQUIRE(c.time_ms[0] > 1000);
  REQUIRE(c.time_ms[0] <= 1100);
  REQUIRE(c.remaining(to_player(0)) == c.time_ms[0]);
}

TEST_CASE("stop() is a no-op if the given player isn't the active one",
          "[clock]") {
  BughouseClock c;
  c.set(1000, 100);
  c.start(to_player(0));
  c.stop(to_player(1)); // player 1 isn't active and should not affect anything

  REQUIRE(c.active_player == 0);
  REQUIRE(c.time_ms[1] == 1000);
}

TEST_CASE("flagged()/any_flagged() detect exhausted time", "[clock]") {
  BughouseClock c;
  c.set(0, 0);

  for (int i = 0; i < 4; i++)
    REQUIRE(c.flagged(to_player(i)));
  REQUIRE(c.any_flagged());
}

TEST_CASE("flagged() is false with healthy time remaining", "[clock]") {
  BughouseClock c;
  c.set(60000, 0);

  for (int i = 0; i < 4; i++)
    REQUIRE_FALSE(c.flagged(to_player(i)));
  REQUIRE_FALSE(c.any_flagged());
}

TEST_CASE("active player ticking down to zero eventually flags", "[clock]") {
  BughouseClock c;
  c.set(30, 0);
  c.start(to_player(2));

  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  REQUIRE(c.flagged(to_player(2)));
  REQUIRE(c.any_flagged());
}