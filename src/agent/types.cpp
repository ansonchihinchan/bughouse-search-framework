  #include "agent/types.h"
  #include "communication/generator.h"
  #include "eval/bughouse.h"
  #include "search/alpha_beta_search.h"
  #include "search/null_move_search.h"
  #include "search/pvs.h"
  #include <stdexcept>
  #include <utility>

  namespace {
  std::unique_ptr<Search> make_search(SearchAlgorithm algorithm,
                                      const Evaluator &evaluator,
                                      TranspositionTable &tt,
                                      const SearchParams &params) {
    switch (algorithm) {
    case SearchAlgorithm::AlphaBeta:
      return std::make_unique<AlphaBetaSearch>(evaluator, tt, params);
    case SearchAlgorithm::PVS:
      return std::make_unique<PVS>(evaluator, tt, params);
    case SearchAlgorithm::NullMove:
      return std::make_unique<NullMoveSearch>(evaluator, tt, params);
    }
    throw std::invalid_argument("unsupported search algorithm");
  }

  class IndependentAgent : public Agent {
  public:
    explicit IndependentAgent(AgentConfig config)
        : Agent(std::move(config), std::make_unique<BughouseEvaluator>(
                                      BughouseEvaluationConfig::independent())) {
    }

    AgentType type() const override { return AgentType::Independent; }

    std::string_view name() const override { return "independent"; }
  };

  class RequestAgent : public Agent {
  public:
    RequestAgent(AgentConfig config, Channel &channel)
        : Agent(std::move(config), std::make_unique<BughouseEvaluator>(
                                      BughouseEvaluationConfig::request())),
          channel_(channel) {}

    AgentType type() const override { return AgentType::Request; }
    std::string_view name() const override { return "request"; }

  protected:
    CommunicationContext communication_context(const BughouseState &game,
                                              PlayerId player) const override {
      return make_communication_context(game.position, player, channel_);
    }

    std::optional<Message>
    make_outgoing_message(const SearchResult &result,
                          const BughousePosition &resulting_position,
                          PlayerId player) override {
      if (result.best_move.is_none())
        return std::nullopt;
      Message message =
          generator_.generate_message(result, resulting_position, player);
      channel_.send(player, message);
      return message;
    }

  private:
    Channel &channel_;
    Generator generator_;
  };

  class SharedValueAgent : public Agent {
  public:
    explicit SharedValueAgent(AgentConfig config)
        : Agent(std::move(config),
                std::make_unique<BughouseEvaluator>(
                    BughouseEvaluationConfig::shared_value())) {}

    AgentType type() const override { return AgentType::SharedValue; }
    std::string_view name() const override { return "shared_value"; }
  };
  } // namespace

  Agent::Agent(AgentConfig config, std::unique_ptr<Evaluator> evaluator)
      : config_(std::move(config)), evaluator_(std::move(evaluator)),
        tt_(config_.transposition_table_mb),
        search_(make_search(config_.search, *evaluator_, tt_,
                            config_.search_params)) {}

  CommunicationContext Agent::communication_context(const BughouseState &game,
                                                    PlayerId player) const {
    (void)game;
    (void)player;
    return {};
  }

  std::optional<Message>
  Agent::make_outgoing_message(const SearchResult &result,
                              const BughousePosition &resulting_position,
                              PlayerId player) {
    (void)result;
    (void)resulting_position;
    (void)player;
    return std::nullopt;
  }

  AgentOutput Agent::choose_move(const BughouseState &game, PlayerId player,
                                const SearchLimits &limits,
                                std::stop_token stop_token) {
    CommunicationContext communication = communication_context(game, player);
    std::array<int64_t, PLAYER_NO> remaining{};
    for (int i = 0; i < PLAYER_NO; i++)
      remaining[i] = game.clock.remaining(to_player(i));
    SearchContext context =
        make_context(remaining, player, communication,
                    communication_hash(communication), &game.history);

    SearchResult result =
        search_->search(game.position, context, limits, stop_token);

    BughousePosition resulting_position = game.position;
    if (!result.best_move.is_none() &&
        resulting_position.boards[board_of(player)].is_legal(result.best_move))
      apply_move(resulting_position, player, result.best_move);

    std::optional<Message> outgoing =
        make_outgoing_message(result, resulting_position, player);
    return AgentOutput{std::move(result), std::move(outgoing)};
  }

  // TODO：SacrificeAgent
  std::unique_ptr<Agent> make_agent(const AgentConfig &config, Channel *channel) {
    switch (config.type) {
    case AgentType::Independent:
      return std::make_unique<IndependentAgent>(config);
    case AgentType::Request:
      if (!channel)
        throw std::invalid_argument("request agent requires a channel");
      return std::make_unique<RequestAgent>(config, *channel);
    case AgentType::SharedValue:
      return std::make_unique<SharedValueAgent>(config);
    }
    throw std::invalid_argument("unsupported agent");
  }