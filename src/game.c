#include "game.h"
#include <ctype.h>
#include <stddef.h>

static const char *const strategy_names[STRATEGY_COUNT] = {
    "always-cooperate", "mostly-cheat", "joker", "copycat", "grudger", "copykitten"
};

const char *strategy_name(strategy_t strategy) {
    return strategy >= ALWAYS_COOPERATE && strategy < STRATEGY_COUNT
        ? strategy_names[strategy] : "unknown";
}

int strategy_parse(const char *name, strategy_t *result) {
    if (!name || !result) return 0;
    for (int i = 0; i < STRATEGY_COUNT; ++i) {
        const char *a = name, *b = strategy_names[i];
        while (*a && *b) {
            char ca = (char)tolower((unsigned char)*a);
            char cb = (char)tolower((unsigned char)*b);
            if (ca == '_') ca = '-';
            if (ca != cb) break;
            ++a;
            ++b;
        }
        if (*a == '\0' && *b == '\0') {
            *result = (strategy_t)i;
            return 1;
        }
    }
    return 0;
}

/* xorshift32: deterministic for fixed seeds, not cryptographically secure. */
void rng_seed(rng_t *rng, uint32_t seed) {
    rng->state = seed == 0 ? UINT32_C(0x6d2b79f5) : seed;
}

static uint32_t rng_next(rng_t *rng) {
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng->state = x;
}

double rng_uniform(rng_t *rng) {
    return (double)(rng_next(rng) >> 8) / 16777216.0;
}

void score_round(move_t a, move_t b, int *points_a, int *points_b) {
    if (a == COOPERATE && b == COOPERATE) {
        *points_a = 2; *points_b = 2;
    } else if (a == DEFECT && b == COOPERATE) {
        *points_a = 3; *points_b = -1;
    } else if (a == COOPERATE && b == DEFECT) {
        *points_a = -1; *points_b = 3;
    } else {
        *points_a = 0; *points_b = 0;
    }
}

void agent_init(agent_t *agent, strategy_t strategy) {
    agent->strategy = strategy;
    agent->rounds_seen = 0;
    agent->opponent_last_move = COOPERATE;
    agent->consecutive_opponent_defections = 0;
    agent->opponent_ever_defected = 0;
}

move_t agent_choose(const agent_t *agent, rng_t *rng, double joker_probability) {
    switch (agent->strategy) {
        case ALWAYS_COOPERATE:
            return COOPERATE;
        case MOSTLY_CHEAT:
            return agent->rounds_seen == 0 ? COOPERATE : DEFECT;
        case JOKER:
            return rng_uniform(rng) < joker_probability ? COOPERATE : DEFECT;
        case COPYCAT:
            return agent->rounds_seen == 0 ? COOPERATE : agent->opponent_last_move;
        case GRUDGER:
            return agent->opponent_ever_defected ? DEFECT : COOPERATE;
        case COPYKITTEN:
            return agent->consecutive_opponent_defections >= 2 ? DEFECT : COOPERATE;
        default:
            return COOPERATE;
    }
}

void agent_observe(agent_t *agent, move_t opponent_move) {
    agent->opponent_last_move = opponent_move;
    if (opponent_move == DEFECT) {
        agent->opponent_ever_defected = 1;
        ++agent->consecutive_opponent_defections;
    } else {
        agent->consecutive_opponent_defections = 0;
    }
    ++agent->rounds_seen;
}

game_result_t play_game(strategy_t a, strategy_t b, game_config_t config, rng_t *rng) {
    agent_t player_a, player_b;
    agent_init(&player_a, a);
    agent_init(&player_b, b);
    game_result_t out = {0};
    for (unsigned round = 0; round < config.rounds; ++round) {
        /* Decisions are made before either agent observes this round's opponent move. */
        move_t move_a = agent_choose(&player_a, rng, config.joker_probability);
        move_t move_b = agent_choose(&player_b, rng, config.joker_probability);
        if (config.noise > 0.0 && rng_uniform(rng) < config.noise)
            move_a = move_a == COOPERATE ? DEFECT : COOPERATE;
        if (config.noise > 0.0 && rng_uniform(rng) < config.noise)
            move_b = move_b == COOPERATE ? DEFECT : COOPERATE;
        int score_a, score_b;
        score_round(move_a, move_b, &score_a, &score_b);
        out.score_a += score_a;
        out.score_b += score_b;
        out.cooperations_a += (unsigned)(move_a == COOPERATE);
        out.cooperations_b += (unsigned)(move_b == COOPERATE);
        agent_observe(&player_a, move_b);
        agent_observe(&player_b, move_a);
    }
    return out;
}
