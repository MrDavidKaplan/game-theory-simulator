#ifndef GAME_THEORY_GAME_H
#define GAME_THEORY_GAME_H

#include <stdint.h>

typedef enum { DEFECT = 0, COOPERATE = 1 } move_t;
typedef enum {
    ALWAYS_COOPERATE = 0,
    MOSTLY_CHEAT,
    JOKER,
    COPYCAT,
    GRUDGER,
    COPYKITTEN,
    STRATEGY_COUNT
} strategy_t;

typedef struct { uint32_t state; } rng_t;

typedef struct {
    strategy_t strategy;
    unsigned rounds_seen;
    move_t opponent_last_move;
    unsigned consecutive_opponent_defections;
    int opponent_ever_defected;
} agent_t;

typedef struct {
    unsigned rounds;
    double noise;
    double joker_probability;
} game_config_t;

typedef struct {
    int64_t score_a;
    int64_t score_b;
    unsigned cooperations_a;
    unsigned cooperations_b;
} game_result_t;

const char *strategy_name(strategy_t strategy);
int strategy_parse(const char *name, strategy_t *result);
void rng_seed(rng_t *rng, uint32_t seed);
double rng_uniform(rng_t *rng);
void score_round(move_t a, move_t b, int *points_a, int *points_b);
void agent_init(agent_t *agent, strategy_t strategy);
move_t agent_choose(const agent_t *agent, rng_t *rng, double joker_probability);
void agent_observe(agent_t *agent, move_t opponent_move);
game_result_t play_game(strategy_t a, strategy_t b, game_config_t config, rng_t *rng);

#endif
