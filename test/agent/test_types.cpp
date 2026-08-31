#include <catch2/catch_all.hpp>

#include "agent/experiment.h"
#include "agent/types.h"
#include "communication/generator.h"
#include "game/movegen.h"
#include <algorithm>
#include <stop_token>

namespace {
SearchLimits shallow_limits(int depth = 1) {
  SearchLimits limits;
  limits.max_depth = depth;
  return limits;
}

AgentOutput decide(Agent &agent, const BughouseState &game, PlayerId player,
                   int depth = 1) {
  std::stop_source stop;
  return agent.choose_move(game, player, shallow_limits(depth),
                           stop.get_token());
}

bool legal_for(const BughouseState &game, PlayerId player, Move move) {
  auto legal = generate_legal_moves(game.position, player);
  return std::find(legal.begin(), legal.end(), move) != legal.end();
}
} // namespace

TEST_CASE("agent strategy factory selects each core strategy",
          "[agent][strategy]") {
  Channel channel;
  AgentConfig config;
  config.seed = 42;

  config.type = AgentType::Independent;
  auto independent = make_agent(config);
  REQUIRE(independent->type() == AgentType::Independent);
  REQUIRE(independent->name() == "independent");
  REQUIRE(independent->seed() == 42);

  config.type = AgentType::Request;
  auto request = make_agent(config, &channel);
  REQUIRE(request->type() == AgentType::Request);
  REQUIRE(request->name() == "request");

  config.type = AgentType::SharedValue;
  auto shared = make_agent(config);
  REQUIRE(shared->type() == AgentType::SharedValue);
  REQUIRE(shared->name() == "shared_value");

  config.type = AgentType::Sacrifice;
  auto sacrifice = make_agent(config);
  REQUIRE(sacrifice->type() == AgentType::Sacrifice);
  REQUIRE(sacrifice->name() == "sacrifice");
}

TEST_CASE("request strategy requires an explicit communication channel",
          "[agent][strategy][communication]") {
  AgentConfig config;
  config.type = AgentType::Request;
  REQUIRE_THROWS_AS(make_agent(config), std::invalid_argument);
}

TEST_CASE("search algorithm selection remains configurable at agent boundary",
          "[agent][strategy][search]") {
  AgentConfig config;
  config.type = AgentType::Independent;

  config.search = SearchAlgorithm::AlphaBeta;
  REQUIRE(make_agent(config)->search().name() == "alpha_beta");
  config.search = SearchAlgorithm::PVS;
  REQUIRE(make_agent(config)->search().name() == "pvs");
  config.search = SearchAlgorithm::NullMove;
  REQUIRE(make_agent(config)->search().name() == "null_move");
}

TEST_CASE("independent strategy chooses legally without communication",
          "[agent][strategy][independent]") {
  BughouseState game;
  BughousePosition before = game.position;
  AgentConfig config;
  config.type = AgentType::Independent;
  auto strategy = make_agent(config);

  AgentOutput output = decide(*strategy, game, to_player(0));

  REQUIRE(legal_for(game, to_player(0), output.search_result.best_move));
  REQUIRE_FALSE(output.outgoing_message.has_value());
  REQUIRE(game.position.boards == before.boards);
  REQUIRE(game.position.pockets == before.pockets);
}

TEST_CASE("request strategy publishes its generated request",
          "[agent][strategy][request][communication]") {
  BughouseState game;
  BughousePosition before = game.position;
  Channel channel;
  AgentConfig config;
  config.type = AgentType::Request;
  auto strategy = make_agent(config, &channel);

  AgentOutput output = decide(*strategy, game, to_player(0));

  REQUIRE(output.outgoing_message.has_value());
  const Message &outgoing = *output.outgoing_message;
  REQUIRE(outgoing.sender == to_player(0));
  REQUIRE(outgoing.piece_request.piece != NO_PIECE_TYPE);
  Message delivered = channel.latest(to_player(0));
  REQUIRE(delivered.sender == outgoing.sender);
  REQUIRE(delivered.piece_request.piece == outgoing.piece_request.piece);
  REQUIRE(delivered.strat_request.strat == outgoing.strat_request.strat);
  REQUIRE(game.position.boards == before.boards);
  REQUIRE(game.position.pockets == before.pockets);
}

TEST_CASE("only request agents publish outgoing communication",
          "[agent][strategy][communication]") {
  BughouseState game;

  AgentConfig independent_config;
  independent_config.type = AgentType::Independent;
  auto independent = make_agent(independent_config);
  REQUIRE_FALSE(decide(*independent, game, to_player(0)).outgoing_message);

  AgentConfig shared_config;
  shared_config.type = AgentType::SharedValue;
  auto shared = make_agent(shared_config);
  REQUIRE_FALSE(decide(*shared, game, to_player(0)).outgoing_message);
}

TEST_CASE("request messages identify their sender and resulting move number",
          "[agent][strategy][request][communication]") {
  BughouseState game;
  Move white_move = generate_legal_moves(game.position, to_player(0)).front();
  game.make_move(to_player(0), white_move);
  const int before_move_no = game.position.boards[0].fullMove;

  Channel channel;
  AgentConfig config;
  config.type = AgentType::Request;
  auto request = make_agent(config, &channel);
  AgentOutput output = decide(*request, game, to_player(1));

  REQUIRE(output.outgoing_message);
  REQUIRE(output.outgoing_message->sender == to_player(1));
  REQUIRE(output.outgoing_message->move_no == before_move_no + 1);
  REQUIRE(channel.latest(to_player(1)).move_no == before_move_no + 1);
}

TEST_CASE("request agent forwards clock context to communication generation",
          "[agent][strategy][request][communication]") {
  BughouseState game;
  game.clock.time_ms[to_int(to_player(2))] = 30'000;
  game.clock.time_ms[to_int(to_player(3))] = 5'000;

  Channel channel;
  AgentConfig config;
  config.type = AgentType::Request;
  auto request = make_agent(config, &channel);
  AgentOutput output = decide(*request, game, to_player(0));

  REQUIRE(output.outgoing_message);
  REQUIRE(output.outgoing_message->strat_request.strat == StrategyType::Flag);
}

TEST_CASE("request-aware evaluator observes a delivered partner request",
          "[agent][strategy][request][communication]") {
  BughousePosition position;
  position.pockets[0].add(KNIGHT);
  std::array<int64_t, PLAYER_NO> remaining{};
  CommunicationContext quiet{};
  CommunicationContext requested{};
  requested.message.sender = to_player(2);
  requested.message.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  AgentConfig config;
  config.type = AgentType::Request;
  Channel channel;
  auto strategy = make_agent(config, &channel);

  int quiet_score =
      strategy->evaluator().evaluate(position, to_player(0), remaining, quiet);
  int requested_score = strategy->evaluator().evaluate(position, to_player(0),
                                                       remaining, requested);
  REQUIRE(requested_score != quiet_score);
}

TEST_CASE("request strategy consumes the partner message from its channel",
          "[agent][strategy][request][communication]") {
  BughouseState game;
  game.position.pockets[0].add(KNIGHT);
  AgentConfig config;
  config.type = AgentType::Request;

  Channel quiet_channel;
  auto quiet_strategy = make_agent(config, &quiet_channel);

  Channel request_channel;
  Message request;
  request.sender = to_player(2);
  request.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};
  request_channel.send(to_player(2), request);
  auto request_strategy = make_agent(config, &request_channel);

  AgentOutput quiet = decide(*quiet_strategy, game, to_player(0));
  AgentOutput requested = decide(*request_strategy, game, to_player(0));
  REQUIRE(requested.search_result.score != quiet.search_result.score);
}

TEST_CASE("shared-value profile responds to partner-board utility while the "
          "independent profile does not",
          "[agent][strategy][shared_value]") {
  BughousePosition baseline;
  BughousePosition partner_advantage = baseline;
  partner_advantage.boards[1].load_fen("4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1");
  std::array<int64_t, PLAYER_NO> remaining{};
  CommunicationContext communication{};

  AgentConfig independent_config;
  independent_config.type = AgentType::Independent;
  auto independent = make_agent(independent_config);

  AgentConfig shared_config;
  shared_config.type = AgentType::SharedValue;
  auto shared = make_agent(shared_config);

  int independent_before = independent->evaluator().evaluate(
      baseline, to_player(0), remaining, communication);
  int independent_after = independent->evaluator().evaluate(
      partner_advantage, to_player(0), remaining, communication);
  REQUIRE(independent_after == independent_before);

  int shared_before = shared->evaluator().evaluate(baseline, to_player(0),
                                                   remaining, communication);
  int shared_after = shared->evaluator().evaluate(
      partner_advantage, to_player(0), remaining, communication);
  REQUIRE(shared_after != shared_before);
}

TEST_CASE("equal seeded configurations make reproducible decisions",
          "[agent][strategy][experiment]") {
  BughouseState game;
  AgentConfig config;
  config.type = AgentType::Independent;
  config.search = SearchAlgorithm::PVS;
  config.seed = 2026;

  auto first = make_agent(config);
  auto second = make_agent(config);
  AgentOutput first_output = decide(*first, game, to_player(0), 2);
  AgentOutput second_output = decide(*second, game, to_player(0), 2);

  REQUIRE(first_output.search_result.best_move ==
          second_output.search_result.best_move);
  REQUIRE(first_output.search_result.score ==
          second_output.search_result.score);
}

TEST_CASE("experiment configuration builds a reproducible mixed-strategy "
          "roster",
          "[agent][experiment]") {
  ExperimentConfig config;
  config.seed = 99;
  config.agent_configs[0].type = AgentType::Independent;
  config.agent_configs[1].type = AgentType::Request;
  config.agent_configs[2].type = AgentType::SharedValue;
  config.agent_configs[3].type = AgentType::Request;

  AgentStrategyExperiment first(config);
  AgentStrategyExperiment second(config);

  REQUIRE(first.agent(to_player(0)).type() == AgentType::Independent);
  REQUIRE(first.agent(to_player(1)).type() == AgentType::Request);
  REQUIRE(first.agent(to_player(2)).type() == AgentType::SharedValue);
  REQUIRE(first.agent(to_player(3)).type() == AgentType::Request);
  for (int i = 0; i < PLAYER_NO; i++)
    REQUIRE(first.agent(to_player(i)).seed() ==
            second.agent(to_player(i)).seed());
}

TEST_CASE("request-aware evaluator does not leak a partner request onto the "
          "opponent's node",
          "[agent][strategy][request][communication][team_perspective]") {
  BughousePosition position;
  position.pockets[1].add(KNIGHT);
  std::array<int64_t, PLAYER_NO> remaining{};

  CommunicationContext quiet{};
  CommunicationContext requested{};
  quiet.origin_player = to_player(0);
  requested.origin_player = to_player(0);
  requested.message.sender = to_player(2);
  requested.message.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  AgentConfig config;
  config.type = AgentType::Request;
  Channel channel;
  auto strategy = make_agent(config, &channel);

  int origin_quiet =
      strategy->evaluator().evaluate(position, to_player(0), remaining, quiet);
  int origin_requested = strategy->evaluator().evaluate(position, to_player(0),
                                                        remaining, requested);
  REQUIRE(origin_requested != origin_quiet);

  int opponent_quiet =
      strategy->evaluator().evaluate(position, to_player(1), remaining, quiet);
  int opponent_requested = strategy->evaluator().evaluate(
      position, to_player(1), remaining, requested);
  REQUIRE(origin_requested - origin_quiet ==
          -(opponent_requested - opponent_quiet));
}