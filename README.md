# Bughouse Cooperative AI Search Framework

A C++20 search framework for experimenting with cooperative AI strategies in
**bughouse chess** — a four-player, two-board variant where captured pieces
are transferred to your partner's reserve and can be dropped back onto the
board

## Features

- **Rules engine** (`game`)
- **Search** (`search`): alpha-beta / PVS / null-move variants
- **Evaluation** (`eval`): classical evaluator (material, pawn structure, etc.) 
  plus bughouse-specific features (drop threats, pocket material, etc.)
- **Communication** (`communication`): a per-move message (piece request +
  strategy signal) generated from search output and board state
- **Agents** (`agent`): see next section

## Agent strategies

The available strategies are:

- `Independent`: local-board evaluation with no messages or partner utility
- `Request`: local evaluation with explicit piece requests over a shared channel
- `SharedValue`: evaluates local and partner boards as one team objective
- `Sacrifice`: compares the ordinary local move with a set of sacrifical moves 
  , plays the sacrifical move if the move transfers a piece that can be dropped 
  by the partner and improves the resulting team value

## Build

```bash
./compile      # cmake -S . -B build && cmake --build build
./run-test     # ctest --test-dir build --output-on-failure
./run-bench    # ./build/bench/bench_search
```

Requires a C++20 compiler and Catch2 3

## Usage

```bash
# Watch one live self-play game between four PVS Independent agents
./build/bughouse self-play --depth 5 --max-plies 80 --clock real --live --output game.replay

# Step back through the recorded game
./build/bughouse replay game.replay --step

# Homogeneous round-robin: each of the four strategies against itself
./build/bughouse tournament --mode homogeneous --games 20 --depth 5 --max-plies 80 --clock real --seed 2026 --output tournament.csv
```

See `bughouse` with no arguments for the full option list

## Current result

The [`results.csv`](results.csv) shows the result of a tournament flagged
exhaustive, depth 5 and 100-ply.

Out of the 256 games:
- 75 (29.3%) ended with a winner
- 28 (10.9%) Team A won
- 47 (18.4%) Team B won
- 142 (55.5%) reached ply limit
- 39 (15.2%) stopped on invalid agent move
- 75/2769 (2.1%) coordination opportunities produced a coordinated response
- 57/748 (7.6%) drops are classified as wasted

## Known limitations, Improvements to be made

- `AgentConfig::seed` dentical seeds always produce identical games
- Invalid agent moves
- Single-threaded search only
- No implementation of stalling
- Naive classical evaluation

## License

MIT — see [LICENSE](LICENSE)