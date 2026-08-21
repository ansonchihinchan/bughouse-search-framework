#pragma once

#include "agent/tournament.h"
#include <optional>
#include <ostream>
#include <string>

class Observer {
public:
  virtual ~Observer() = default;

  virtual void on_game_start(const BughouseState &initial_state) {}

  virtual void on_ply(const ReplayEvent &event, const BughouseState &state) {
    (void)event;
    (void)state;
  }

  virtual void on_game_end(const SelfPlayResult &result) { (void)result; }
};

class TournamentObserver : public Observer {
public:
  virtual void on_tournament_start(const TournamentConfig &config) {
    (void)config;
  }

  virtual void on_game_end(const TournamentGameRecord &record,
                           const TournamentSummary &summary) {
    (void)record;
    (void)summary;
  }

  virtual void on_tournament_end(const TournamentResult &result) {
    (void)result;
  }
};

class TerminalRenderer {
public:
  std::string render_game_state(
      const BughouseState &state,
      const std::optional<ReplayEvent> &last_event = std::nullopt) const;
  std::string render_game_end(const SelfPlayResult &result) const;
  std::string render_tournament_start(const TournamentConfig &config) const;
  std::string render_tournament_progress(const TournamentGameRecord &record,
                                         const TournamentSummary &summary,
                                         size_t configured_games) const;
  std::string render_tournament_end(const TournamentResult &result) const;

private:
  std::string render_boards(const BughousePosition &position) const;
  std::string render_pockets(const BughousePosition &position) const;
  std::string render_clocks(const BughouseClock &clock) const;
  std::string render_event(const ReplayEvent &event) const;
  std::string render_message(const Message &message) const;
};

class TerminalObserver : public TournamentObserver {
public:
  explicit TerminalObserver(std::ostream &out, bool live = false);

  using TournamentObserver::on_game_end;

  void on_game_start(const BughouseState &initial_state) override;
  void on_ply(const ReplayEvent &event, const BughouseState &state) override;
  void on_game_end(const SelfPlayResult &result) override;
  void on_tournament_start(const TournamentConfig &config) override;
  void on_game_end(const TournamentGameRecord &record,
                   const TournamentSummary &summary) override;
  void on_tournament_end(const TournamentResult &result) override;

private:
  void emit(const std::string &text, bool refresh);

  std::ostream &out_;
  bool live_ = false;
  size_t tournament_games_ = 0;
  std::string last_frame_;
  TerminalRenderer renderer_;
};