# Model specification

## State and action space

Each simulated match consists of two agents A and B and `T` discrete rounds. The CLI also supports two-human play and a human-versus-agent match using the same payoffs. At round `t`, each agent selects an intended action from `{C, D}`. Both actions are selected before either opponent action is observed.

An agent stores only the observations needed by its policy:

- Number of rounds already completed.
- Opponent action in the preceding round.
- Whether its opponent has ever defected.
- Length of the opponent's most recent consecutive defection streak.

The state is initialized from scratch for each match. No information persists between matchups.

## Payoff function

Given actual actions, the reward vector `(r_A, r_B)` is:

| `(A,B)` | `r_A` | `r_B` |
|---|---:|---:|
| `(C,C)` | 2 | 2 |
| `(C,D)` | -1 | 3 |
| `(D,C)` | 3 | -1 |
| `(D,D)` | 0 | 0 |

The cumulative reward for an agent is the sum of its individual per-round rewards. The reported mean points per round is cumulative reward divided by `T`.

## Execution error

Each intended action is flipped independently with probability `epsilon` (`--noise`). For example, at `epsilon = 0.02`, intended cooperation is executed as defection in approximately 2% of decisions. The opponent observes the actual action, not the intended one. No observation-only errors are modeled.

## Randomized behavior

The Joker policy independently intends cooperation with probability `p` (`--joker-prob`). A seeded xorshift32 PRNG supplies pseudorandom choices and execution errors. Since the random stream is deterministic, results can be exactly reproduced by using identical CLI parameters and a fixed seed.

The generator is chosen for simplicity and repeatability, not statistical excellence for rigorous Monte Carlo research. Use a stronger generator when studying subtle statistical effects.

## Tournament scoring

For each ordered pair `(i,j)`, simulate `M` independent matches of `T` rounds with the strategy `i` in the first-player seat. The reported entry for `(i,j)` is:

`mean_score[i,j] = sum of all first-player rewards / (M * T)`

The terminal's `AVERAGE` column is the arithmetic mean of these six matchup scores for strategy `i`—one score per opponent (including itself). Scores can be negative. Higher indicates greater modeled reward per round *under the specified opponent distribution*, not universal dominance.

The exported `cooperation_rate[i,j]` is the proportion of the first player's executed actions that are cooperation, over all `M*T` rounds.

## Known limitations

1. Exactly two actions and fixed payoffs; no changing games or resource constraints.
2. Six simple fixed policies; no adaptation or learning across matches.
3. Single pseudorandom stream; a tournament with another strategy order will produce a different random stream even with the same seed.
4. No confidence intervals, hypothesis tests, or formal convergence guarantees.
5. Tournament ranking reflects uniform opponents and self-play, not an externally estimated strategy population.

For extensions, consider reproducible per-match seeds, more diverse opponents, confidence intervals, policy learning, population dynamics, or a tournament scheduler with configurable mixture weights.
