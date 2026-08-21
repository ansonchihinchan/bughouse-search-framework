#pragma once

#include "agent/temporal.h"
#include "communication/channel.h"
#include "eval/evaluator.h"
#include "game/bughouse.h"
#include "search/search.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string_view>

enum class AgentType { Independent, Request, SharedValue, Sacrifice };

std::string_view agent_type_name(AgentType type);

enum class SearchAlgorithm { AlphaBeta, PVS, NullMove };

struct AgentConfig {
  AgentType type = AgentType::Independent;
  SearchAlgorithm search = SearchAlgorithm::PVS;
  SearchParams search_params{};
  size_t transposition_table_mb = 64;
  uint64_t seed = 0;
  TemporalConfig temporal_config{};
};

struct AgentOutput {
  SearchResult search_result;
  std::optional<Message> outgoing_message;
  struct Metrics {
    size_t sacrifice_attempts = 0;
    size_t sacrifices_accepted = 0;
    size_t temporal_transfers_observed = 0;
    size_t temporal_partner_uses = 0;
  } metrics;
};

class Agent {
public:
  virtual ~Agent() = default;

  Agent(const Agent &) = delete;
  Agent &operator=(const Agent &) = delete;

  AgentOutput choose_move(const BughouseState &game, PlayerId player,
                          const SearchLimits &limits,
                          std::stop_token stop_token);

  virtual AgentType type() const = 0;
  virtual std::string_view name() const = 0;

  uint64_t seed() const { return config_.seed; }
  const Evaluator &evaluator() const { return *evaluator_; }
  const Search &search() const { return *search_; }

protected:
  Agent(AgentConfig config, std::unique_ptr<Evaluator> evaluator);

  virtual CommunicationContext communication_context(const BughouseState &game,
                                                     PlayerId player) const;

  virtual std::optional<Message> make_outgoing_message(
      const SearchResult &result, const BughousePosition &resulting_position,
      PlayerId player, const std::array<int64_t, PLAYER_NO> &remaining);

  virtual SearchResult select_move(const BughouseState &game,
                                   const SearchContext &context,
                                   const SearchLimits &limits,
                                   std::stop_token stop_token);

  AgentConfig config_;
  std::unique_ptr<Evaluator> evaluator_;
  TranspositionTable tt_;
  std::unique_ptr<Search> search_;
  AgentOutput::Metrics last_metrics_{};
};

std::unique_ptr<Agent> make_agent(const AgentConfig &config,
                                  Channel *channel = nullptr);