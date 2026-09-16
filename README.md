# Bughouse Cooperative AI Search Framework

A C++20 search framework for experimenting with cooperative AI strategies in **bughouse chess**: a four-player, two-board chess variant in which captured pieces are transferred to a partner's reserve and can later be dropped onto the partner's board.

## Features

- **Rules engine** (`game`)
  - Implements the core board, move-generation, pocket, and game-state logic.
- **Search** (`search`)
  - Includes alpha-beta, Principal Variation Search (PVS), and null-move search
    variants.
- **Evaluation** (`eval`)
  - Provides a classical evaluator covering features such as material and pawn structure.
  - Includes bughouse-specific evaluation features such as pocket material and drop threats.
- **Communication** (`communication`)
  - Produces a per-move communication message containing a piece request and a strategy signal.
  - Uses search output and the current board state to construct the message.
- **Agents** (`agent`)
  - Provides several agent strategies for experimenting with local evaluation, communication, shared team value, and sacrifices.

## Agent strategies

The framework currently provides the following strategies:

- **`Independent`**
  - Uses local-board evaluation.
  - Does not use communication messages or partner utility.

- **`Request`**
  - Uses local-board evaluation.
  - Sends explicit piece requests through a shared communication channel.

- **`SharedValue`**
  - Evaluates the local and partner boards as a single team objective.
  - Attempts to account for the value of both teammates' positions.

- **`Sacrifice`**
  - Compares the ordinary local move against a set of candidate sacrificial
    moves.
  - Plays a sacrificial move when it transfers a piece that the partner can
    potentially use and the resulting team value improves.

These strategies are experimental. Differences in observed tournament results should not be interpreted as causal evidence of strategy superiority without repeated trials under controlled conditions.

## Build

The project requires:

- A C++20-compatible compiler
- CMake
- Catch2 3 for the test suite

Convenience scripts are provided:

```bash
./compile       # cmake -S . -B build && cmake --build build
./run-test      # ctest --test-dir build --output-on-failure
./run-bench     # ./build/bench/bench_search
```

## Usage

### Live self-play

Watch a live self-play game involving four PVS `Independent` agents:

```bash
./build/bughouse self-play \
  --depth 5 \
  --max-plies 80 \
  --clock real \
  --live \
  --output game.replay
```

### Replay a recorded game

Step through the recorded game:

```bash
./build/bughouse replay game.replay --step
```

### Homogeneous tournament

Run a homogeneous tournament in which all four agents use the same strategy:

```bash
./build/bughouse tournament \
  --mode homogeneous \
  --games 20 \
  --depth 5 \
  --max-plies 80 \
  --clock real \
  --seed 2026 \
  --output tournament.csv
```

Run `./build/bughouse` without arguments to view the full command-line option list.

## Current results

The [`latest_exhaustive_2026.csv`](latest_exhaustive_2026.csv) shows the result of the following command:
```bash
./build/bughouse tournament \
  --mode exhaustive \
  --depth 5 \
  --max-plies 100 \
  --clock real \
  --seed 2026 \
  --output latest_exhaustive_2026.csv
```

### Game outcomes

| Outcome | Games | Percentage |
|---|---:|---:|
| Ongoing / reached the ply limit | 120 | 46.7% |
| Team A wins | 55 | 21.4% |
| Team B wins | 49 | 19.1% |
| Draws | 32 | 12.5% |

Of the **136 games that reached `game_over`**, there were 104 decisive results
and 32 draws. The remaining 120 games terminated at the 100-ply limit. The
average game length was 87.0 plies, with a median of 98 plies.

#### Independent

| Outcome | Games | Percentage |
|---|---:|---:|
| Games won | 39 | 15.2% |
| Games drawn | 33 | 12.9% |
| Games lost | 51 | 19.9% |
| Games unfinished | 133 | 52.0% |

#### Request
| Outcome | Games | Percentage |
|---|---:|---:|
| Games won | 64 | 25.0 % |
| Games drawn | 31 | 12.1% |
| Games lost | 33 | 12.9% |
| Games unfinished | 128 | 50.0% |

#### SharedValue
| Outcome | Games | Percentage |
|---|---:|---:|
| Games won | 54 | 21.1% |
| Games drawn | 38 | 14.8% |
| Games lost | 56 | 21.9% |
| Games unfinished | 108 | 42.2% |

#### Sacrifice
| Outcome | Games | Percentage |
|---|---:|---:|
| Games won | 51 | 19.9% |
| Games drawn | 26 | 10.2% |
| Games lost | 68 | 26.6% |
| Games unfinished | 111 | 43.4% |

### Communication and coordination

| Metric | Total | Average per game |
|---|---:|---:|
| Messages | 5,469 | 21.36 |
| Piece requests | 5,458 | 21.32 |
| Strategy requests | 5,469 | 21.36 |
| Fulfilled requests | 189 | 0.74 |
| Piece transfers | 5,087 | 19.87 |
| Coordination opportunities | 2,849 | 11.13 |
| Coordinated responses | 60 | 0.23 |

The overall piece-request fulfilment rate was **3.46%** (`189 / 5,458`).
The coordination response rate was **2.11%** (`60 / 2,849`).

### Sacrifice and partner interactions

| Metric | Total | Average per game |
|---|---:|---:|
| Sacrifice attempts | 2,373 | 9.27 |
| Accepted sacrifices | 22 | 0.09 |
| Modelled transfers | 1,590 | 6.21 |
| Modelled partner uses | 76 | 0.30 |
| Actual partner uses | 4 | 0.02 |
| Successful temporal sacrifices | 4 | 0.02 |

Only **22 of 2,373 sacrifice attempts** were accepted, an acceptance rate of approximately **0.93%**. There were four recorded successful temporal sacrifices and four actual partner uses in the dataset.

### Drop efficiency

| Metric | Result |
|---|---:|
| Total drops | 927 |
| Wasted drops | 55 |
| Weighted wasted-drop rate | 5.93% |
| Average drops per game | 3.62 |
| Average wasted drops per game | 0.21 |

### Search-budget fallbacks

- **47 of 257 games** contained at least one budget fallback: 18.3%.
- There were **182 budget fallbacks** in total.
- The average was **0.71 fallbacks per game**.

## Known limitations and planned improvements

- **Seed handling**
  - Identical `AgentConfig::seed` values can produce identical games.
  - Improve seed derivation and independent random-stream handling across agents and games.

- **Search parallelism**
  - Search is currently single-threaded.
  - Investigate parallel search approaches once correctness and benchmarking are stable.

- **Stalling**
  - Stalling is not currently implemented.
  - Add explicit stalling behaviour and define how it interacts with clocks, communication, and terminal conditions.

- **Evaluation quality**
  - The classical evaluation remains relatively naive.
  - Improve feature calibration and validate bughouse-specific features against controlled positions and game outcomes.

- **Experimental methodology**
  - Use repeated games with multiple seeds for each fixed strategy matchup.
  - Use games with results (unlimited plies).
  - Separate strategy effects from agent-position effects and other matchup variables.

## License

MIT — see [LICENSE](LICENSE).