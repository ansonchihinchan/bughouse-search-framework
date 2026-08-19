#pragma once

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

enum class AgentType { Independent, Request, SharedValue };

enum class SearchAlgorithm { AlphaBeta, PVS, NullMove };

struct AgentConfig {
  AgentType type = AgentType::Independent;
  SearchAlgorithm search = SearchAlgorithm::PVS;
  SearchParams search_params{};
  size_t transposition_table_mb = 64;
  uint64_t seed = 0;
};

struct AgentOutput {
  SearchResult search_result;
  std::optional<Message> outgoing_message;
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

  virtual CommunicationContext
  communication_context(const BughouseState &game, PlayerId player) const;

  virtual std::optional<Message>
  make_outgoing_message(const SearchResult &result,
                        const BughousePosition &resulting_position,
                        PlayerId player);

  AgentConfig config_;
  std::unique_ptr<Evaluator> evaluator_;
  TranspositionTable tt_;
  std::unique_ptr<Search> search_;
};

std::unique_ptr<Agent>
make_agent(const AgentConfig &config, Channel *channel = nullptr);