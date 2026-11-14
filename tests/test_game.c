#include "game.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_payoffs(void) {
    int a, b;
    score_round(COOPERATE, COOPERATE, &a, &b); assert(a == 2 && b == 2);
    score_round(COOPERATE, DEFECT, &a, &b); assert(a == -1 && b == 3);
    score_round(DEFECT, COOPERATE, &a, &b); assert(a == 3 && b == -1);
    score_round(DEFECT, DEFECT, &a, &b); assert(a == 0 && b == 0);
}

static void test_names(void) {
    strategy_t s;
    assert(strategy_parse("copycat", &s) && s == COPYCAT);
    assert(strategy_parse("ALWAYS_COOPERATE", &s) && s == ALWAYS_COOPERATE);
    assert(strategy_parse("CopyKitten", &s) && s == COPYKITTEN);
    assert(!strategy_parse("missing-strategy", &s));
    assert(!strcmp(strategy_name(JOKER), "joker"));
}

static void test_strategies(void) {
    agent_t a;
    rng_t rng;
    rng_seed(&rng, 42);

    agent_init(&a, ALWAYS_COOPERATE);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
    agent_observe(&a, DEFECT);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);

    agent_init(&a, MOSTLY_CHEAT);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
    agent_observe(&a, COOPERATE);
    assert(agent_choose(&a, &rng, .5) == DEFECT);

    agent_init(&a, JOKER);
    for (int i = 0; i < 100; ++i) {
        assert(agent_choose(&a, &rng, 0.0) == DEFECT);
        assert(agent_choose(&a, &rng, 1.0) == COOPERATE);
    }

    agent_init(&a, COPYCAT);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
    agent_observe(&a, DEFECT);
    assert(agent_choose(&a, &rng, .5) == DEFECT);
    agent_observe(&a, COOPERATE);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);

    agent_init(&a, GRUDGER);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
    agent_observe(&a, DEFECT);
    assert(agent_choose(&a, &rng, .5) == DEFECT);
    agent_observe(&a, COOPERATE);
    assert(agent_choose(&a, &rng, .5) == DEFECT);

    agent_init(&a, COPYKITTEN);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
    agent_observe(&a, DEFECT);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
    agent_observe(&a, DEFECT);
    assert(agent_choose(&a, &rng, .5) == DEFECT);
    agent_observe(&a, COOPERATE);
    assert(agent_choose(&a, &rng, .5) == COOPERATE);
}

static void test_games(void) {
    rng_t rng;
    rng_seed(&rng, 10);
    game_config_t config = {10, 0.0, 0.5};
    game_result_t g = play_game(ALWAYS_COOPERATE, ALWAYS_COOPERATE, config, &rng);
    assert(g.score_a == 20 && g.score_b == 20);
    assert(g.cooperations_a == 10 && g.cooperations_b == 10);

    config.rounds = 3;
    g = play_game(MOSTLY_CHEAT, ALWAYS_COOPERATE, config, &rng);
    assert(g.score_a == 8 && g.score_b == 0);
    assert(g.cooperations_a == 1 && g.cooperations_b == 3);

    config.rounds = 10;
    config.noise = 1.0;
    g = play_game(ALWAYS_COOPERATE, ALWAYS_COOPERATE, config, &rng);
    assert(g.score_a == 0 && g.score_b == 0);
    assert(g.cooperations_a == 0 && g.cooperations_b == 0);

    config.noise = 0.0;
    rng_seed(&rng, 42);
    game_result_t first = play_game(JOKER, COPYCAT, config, &rng);
    rng_seed(&rng, 42);
    game_result_t second = play_game(JOKER, COPYCAT, config, &rng);
    assert(first.score_a == second.score_a && first.score_b == second.score_b);
    assert(first.cooperations_a == second.cooperations_a);

    /* State is per match: a new match must not inherit a previous grudge. */
    g = play_game(GRUDGER, ALWAYS_COOPERATE, config, &rng);
    assert(g.score_a == 20 && g.score_b == 20);
}

int main(void) {
    test_payoffs();
    test_names();
    test_strategies();
    test_games();
    puts("All game-theory simulator tests passed.");
    return 0;
}
