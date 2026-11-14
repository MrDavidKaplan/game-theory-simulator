#include "game.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    game_config_t config;
    uint32_t seed;
    unsigned matches;
    strategy_t p1;
    strategy_t p2;
    const char *csv_path;
    int mode; /* 0 = agent vs agent, 1 = tournament, 2 = human vs AI, 3 = two humans */
} options_t;

static void usage(FILE *out) {
    fprintf(out,
        "Game Theory Simulator — repeated Prisoner's Dilemma\n\n"
        "Usage:\n"
        "  game-theory play [options]\n"
        "  game-theory tournament [options]\n"
        "  game-theory vs-ai [options]\n"
        "  game-theory human [options]\n\n"
        "Options:\n"
        "  --p1 NAME          First strategy for play (default copycat)\n"
        "  --p2 NAME          Second strategy for play (default grudger)\n"
        "  --rounds N         Rounds per match, 1–100000 (default 200)\n"
        "  --matches N        Matches per ordered pair in tournament (default 100)\n"
        "  --seed N           Reproducible unsigned 32-bit seed (default 42)\n"
        "  --noise X          Independent probability of action flip, 0–1 (default 0)\n"
        "  --joker-prob X     Joker's probability to cooperate, 0–1 (default 0.5)\n"
        "  --csv PATH         Export tournament matchup results to CSV\n"
        "  --help             Show this help\n\n"
        "Strategies: always-cooperate, mostly-cheat, joker, copycat, grudger, copykitten\n");
}

static int parse_uint(const char *s, unsigned long long max, unsigned long long *value) {
    if (!s || !*s || *s == '-') return 0;
    errno = 0;
    char *end;
    unsigned long long parsed = strtoull(s, &end, 10);
    if (errno || *end || parsed > max) return 0;
    *value = parsed;
    return 1;
}

static int parse_probability(const char *s, double *value) {
    if (!s || !*s) return 0;
    errno = 0;
    char *end;
    double parsed = strtod(s, &end);
    if (errno || *end || !(parsed >= 0.0 && parsed <= 1.0)) return 0;
    *value = parsed;
    return 1;
}

static int read_options(int argc, char **argv, options_t *opts) {
    opts->config = (game_config_t){200, 0.0, 0.5};
    opts->seed = 42;
    opts->matches = 100;
    opts->p1 = COPYCAT;
    opts->p2 = GRUDGER;
    opts->csv_path = NULL;
    opts->mode = 0;

    if (argc < 2) { usage(stderr); return 0; }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "help")) {
        usage(stdout); return -1;
    }
    if (!strcmp(argv[1], "play")) opts->mode = 0;
    else if (!strcmp(argv[1], "tournament")) opts->mode = 1;
    else if (!strcmp(argv[1], "vs-ai")) opts->mode = 2;
    else if (!strcmp(argv[1], "human")) opts->mode = 3;
    else { fprintf(stderr, "Unknown command: %s\n", argv[1]); usage(stderr); return 0; }

    for (int i = 2; i < argc; ++i) {
        if (!strcmp(argv[i], "--help")) { usage(stdout); return -1; }
        if (i + 1 >= argc) { fprintf(stderr, "Expected a value after %s\n", argv[i]); return 0; }
        const char *key = argv[i], *value = argv[++i];
        unsigned long long n;
        if (!strcmp(key, "--p1")) {
            if (!strategy_parse(value, &opts->p1)) goto bad_value;
        } else if (!strcmp(key, "--p2")) {
            if (!strategy_parse(value, &opts->p2)) goto bad_value;
        } else if (!strcmp(key, "--rounds")) {
            if (!parse_uint(value, 100000, &n) || !n) goto bad_value;
            opts->config.rounds = (unsigned)n;
        } else if (!strcmp(key, "--matches")) {
            if (!parse_uint(value, 100000, &n) || !n) goto bad_value;
            opts->matches = (unsigned)n;
        } else if (!strcmp(key, "--seed")) {
            if (!parse_uint(value, UINT32_MAX, &n)) goto bad_value;
            opts->seed = (uint32_t)n;
        } else if (!strcmp(key, "--noise")) {
            if (!parse_probability(value, &opts->config.noise)) goto bad_value;
        } else if (!strcmp(key, "--joker-prob")) {
            if (!parse_probability(value, &opts->config.joker_probability)) goto bad_value;
        } else if (!strcmp(key, "--csv")) {
            opts->csv_path = value;
        } else {
            fprintf(stderr, "Unknown option: %s\n", key);
            return 0;
        }
        continue;
    bad_value:
        fprintf(stderr, "Invalid value for %s: %s\n", key, value);
        return 0;
    }
    if (opts->mode != 1 && opts->csv_path) {
        fprintf(stderr, "--csv is only supported for tournaments\n");
        return 0;
    }
    /* Guard accidental runs that could take hours. */
    if (opts->mode == 1 &&
        (uint64_t)opts->config.rounds * opts->matches * STRATEGY_COUNT * STRATEGY_COUNT > 100000000ULL) {
        fprintf(stderr, "Tournament exceeds the 100 million simulated-round safety limit\n");
        return 0;
    }
    return 1;
}

static void show_play(const options_t *opts, rng_t *rng) {
    game_result_t result = play_game(opts->p1, opts->p2, opts->config, rng);
    double n = (double)opts->config.rounds;
    printf("Match: %s vs %s\n", strategy_name(opts->p1), strategy_name(opts->p2));
    printf("Rounds: %u | Seed: %" PRIu32 " | Noise: %.3f | Joker p: %.3f\n\n",
           opts->config.rounds, opts->seed, opts->config.noise, opts->config.joker_probability);
    printf("%-20s  %12s  %15s  %14s\n", "Strategy", "Total points", "Points / round", "Cooperation");
    printf("%-20s  %12" PRId64 "  %15.3f  %13.1f%%\n", strategy_name(opts->p1),
           result.score_a, result.score_a / n, 100.0 * result.cooperations_a / n);
    printf("%-20s  %12" PRId64 "  %15.3f  %13.1f%%\n", strategy_name(opts->p2),
           result.score_b, result.score_b / n, 100.0 * result.cooperations_b / n);
}

static int show_tournament(const options_t *opts, rng_t *rng) {
    double score[STRATEGY_COUNT][STRATEGY_COUNT];
    double cooperation[STRATEGY_COUNT][STRATEGY_COUNT];
    FILE *csv = NULL;
    if (opts->csv_path) {
        csv = fopen(opts->csv_path, "w");
        if (!csv) { perror(opts->csv_path); return 0; }
        fprintf(csv, "strategy,opponent,mean_points_per_round,cooperation_rate\n");
    }
    double denom = (double)opts->config.rounds * opts->matches;
    for (int a = 0; a < STRATEGY_COUNT; ++a) {
        for (int b = 0; b < STRATEGY_COUNT; ++b) {
            int64_t total_points = 0;
            uint64_t total_cooperation = 0;
            for (unsigned match = 0; match < opts->matches; ++match) {
                game_result_t result = play_game((strategy_t)a, (strategy_t)b, opts->config, rng);
                total_points += result.score_a;
                total_cooperation += result.cooperations_a;
            }
            score[a][b] = total_points / denom;
            cooperation[a][b] = total_cooperation / denom;
            if (csv) {
                fprintf(csv, "%s,%s,%.6f,%.6f\n", strategy_name((strategy_t)a),
                        strategy_name((strategy_t)b), score[a][b], cooperation[a][b]);
            }
        }
    }
    if (csv && fclose(csv)) { perror("CSV write"); return 0; }

    printf("Round-robin tournament | %u rounds/match | %u matches/opponent | seed %" PRIu32 "\n",
           opts->config.rounds, opts->matches, opts->seed);
    printf("Noise: %.3f | Joker cooperation probability: %.3f\n", opts->config.noise, opts->config.joker_probability);
    printf("Mean points per round by strategy against each opponent (higher is better).\n\n");
    printf("%-20s", "Strategy\\Opponent");
    for (int b = 0; b < STRATEGY_COUNT; ++b) printf(" %8.8s", strategy_name((strategy_t)b));
    printf(" %9s\n", "AVERAGE");
    for (int a = 0; a < STRATEGY_COUNT; ++a) {
        double sum = 0.0;
        printf("%-20s", strategy_name((strategy_t)a));
        for (int b = 0; b < STRATEGY_COUNT; ++b) {
            printf(" %8.3f", score[a][b]);
            sum += score[a][b];
        }
        printf(" %9.3f\n", sum / STRATEGY_COUNT);
    }
    if (csv) printf("\nExported: %s\n", opts->csv_path);
    return 1;
}

/* Human-input modes use the same engine's scoring rules and agent policies. */
static int read_move(const char *name, unsigned round, move_t *move) {
    int n;
    printf("Round %u | %s (1=cooperate, 0=defect): ", round + 1, name);
    fflush(stdout);
    if (scanf("%d", &n) != 1 || (n != 0 && n != 1)) {
        fprintf(stderr, "Expected 0 or 1\n");
        return 0;
    }
    *move = n ? COOPERATE : DEFECT;
    return 1;
}

static move_t executed_move(move_t intended, const options_t *opts, rng_t *rng) {
    if (opts->config.noise > 0 && rng_uniform(rng) < opts->config.noise)
        return intended == COOPERATE ? DEFECT : COOPERATE;
    return intended;
}

static int play_interactive(const options_t *opts, rng_t *rng) {
    int64_t total_a = 0, total_b = 0;
    agent_t computer;
    agent_init(&computer, opts->p2);
    printf("%s | %u rounds | noise %.3f\n",
           opts->mode == 2 ? "Human vs computer" : "Two human players",
           opts->config.rounds, opts->config.noise);
    if (opts->mode == 2) printf("Computer strategy: %s\n", strategy_name(opts->p2));

    for (unsigned i = 0; i < opts->config.rounds; ++i) {
        move_t a, b;
        if (!read_move("Player 1", i, &a)) return 0;
        if (opts->mode == 2) {
            /* Choose before observing the current human move. */
            b = agent_choose(&computer, rng, opts->config.joker_probability);
        } else if (!read_move("Player 2", i, &b)) {
            return 0;
        }
        a = executed_move(a, opts, rng);
        b = executed_move(b, opts, rng);
        int points_a, points_b;
        score_round(a, b, &points_a, &points_b);
        total_a += points_a;
        total_b += points_b;
        printf("  Played: P1=%c P2=%c | Round: %+d/%+d | Total: %" PRId64 "/%" PRId64 "\n",
               a == COOPERATE ? 'C' : 'D', b == COOPERATE ? 'C' : 'D',
               points_a, points_b, total_a, total_b);
        if (opts->mode == 2) agent_observe(&computer, a);
    }
    printf("Final scores: Player 1 = %" PRId64 ", Player 2 = %" PRId64 "\n", total_a, total_b);
    return 1;
}

int main(int argc, char **argv) {
    options_t opts;
    int result = read_options(argc, argv, &opts);
    if (result == -1) return 0;
    if (!result) return 2;
    rng_t rng;
    rng_seed(&rng, opts.seed);
    if (opts.mode == 1) return show_tournament(&opts, &rng) ? 0 : 1;
    if (opts.mode == 2 || opts.mode == 3) return play_interactive(&opts, &rng) ? 0 : 1;
    show_play(&opts, &rng);
    return 0;
}
