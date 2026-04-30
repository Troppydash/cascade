#pragma once

#include "board.hpp"
#include <format>
#include <vector>
#include <chrono>

constexpr int MAX_DEPTH = 128;
constexpr int SS_HEAD = 10;

constexpr int INF = 32000;
constexpr int CHECKMATE = INF - MAX_DEPTH;
constexpr int VALUE_DRAW = 0;
constexpr int VALUE_NONE = INF + 1;
constexpr int UNSEARCHED_DEPTH = -19;
constexpr int UNINIT_DEPTH = -20;
constexpr int NO_FLAG = 0;
constexpr int ALPHA_FLAG = 1;
constexpr int BETA_FLAG = 2;
constexpr int EXACT_FLAG = 3;

std::string score_to_cp(int score) {
    if (score > CHECKMATE)
        return std::format("mate {}", INF - score);
    if (score < -CHECKMATE)
        return std::format("mate {}", -INF - score);

    return std::format("cp {}", score);
}

struct timer {
    int64_t base;
    int64_t optimal;
    int64_t limit;
    bool stopped;

    void start(int64_t optimal, int64_t limit) {
        base = now();
        this->optimal = optimal;
        this->limit = limit;
        stopped = false;
    }

    void check() {
        if (stopped)
            return;

        stopped = now() >= base + limit;
    }

    bool is_stopped() const { return stopped; }

    bool is_optimal_stopped() const { return now() >= base + optimal; }


    static int64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                .count();
    }
};

struct tt {
    struct entry {
        struct data {
            bool hit;
            bool can_use;
            int static_score;
            int score;
            int depth;
            move m;
            int flag;
        };

        int static_score;
        int score;
        int depth;
        move m;
        int flag;

        void reset() {
            static_score = VALUE_NONE;
            score = VALUE_NONE;
            depth = UNINIT_DEPTH;
            m = move::none();
            flag = NO_FLAG;
        }

        // TODO: start here

        data get() {}

        void set() {}
    };

    int size;
    entry *entries;

    explicit tt(int mb) {
        size = mb * 1024 * 1024 / sizeof(entry);
        entries = new entry[size];

        reset();
    }

    void reset() {
        for (int i = 0; i < size; ++i)
            entries[i].reset();
    }

    ~tt() { delete[] entries; }

    entry *get_entry(uint64_t hash) {
        __int128 index = __int128(hash) * __int128(size) >> 64;
        return &entries[index];
    }
};

struct heuristics {
    // main history
};

struct movepick {
    enum stage {
        DROP_PV = 0,
        DROP_INIT,
        DROP_MOVES,

        PV,
        // EXPAND/MOVE that are captures
        CAPTURE_INIT,
        // MOVE and good EXPAND
        GOOD_CAPTURE,

        // non-capture MOVE
        QUIET_INIT,
        QUIET,

        // bad EXPAND
        BAD_EXPAND,

        DONE
    };

    int m_stage;
    move m_pv;
    const board &m_board;
    const heuristics &m_heur;

    int move_ptr[2];
    std::vector<move> moves[2];

    // negamax
    explicit movepick(move pv, const board &board, const heuristics &heur) : m_pv(pv), m_board(board), m_heur(heur) {
        if (m_board.is_drop())
            m_stage = DROP_PV;
        else
            m_stage = PV;
    }

    move next_move() {
        while (true) {
            switch ((stage) m_stage) {
                case DROP_PV: {
                    m_stage++;
                    if (!m_pv.is_none()) {
                        return m_pv;
                    }
                    break;
                }
                case DROP_INIT: {
                    movegen gen{m_board};
                    moves[0] = gen.get_drops();
                    move_ptr[0] = 0;

                    // order moves


                    m_stage++;
                    break;
                }

                case DROP_MOVES: {
                    move_ptr[0] = pick_move(moves[0], move_ptr[0], moves[0].size(), [](auto &) { return true; });
                    if (move_ptr[0] < moves[0].size())
                        return moves[0][move_ptr[0]++];

                    m_stage = DONE;
                    break;
                }

                case PV: {
                    m_stage++;
                    if (!m_pv.is_none()) {
                        return m_pv;
                    }
                    break;
                }
                case CAPTURE_INIT: {
                }
                case GOOD_CAPTURE: {
                }
                case QUIET_INIT: {
                }
                case QUIET: {
                }
                case BAD_EXPAND: {
                }

                case DONE: {
                    return move::none();
                }
            }
        }
    }

    template<typename Pred>
    int pick_move(const std::vector<move> &moves, const int start, const int end, Pred filter) {
        for (int i = start; i < end; ++i) {
            if (!filter(moves[i]))
                continue;

            return i;
        }

        return end;
    }


    static void sort_moves(std::vector<move> &moves, int start, int end,
                           int limit = std::numeric_limits<int16_t>::min()) {
        for (int i = start + 1; i < end; ++i) {
            if (moves[i].score() >= limit) {
                move temp = moves[i];
                int j = i - 1;
                while (j >= start && moves[j].score() < temp.score()) {
                    moves[j + 1] = moves[j];
                    j--;
                }
                moves[j + 1] = temp;
            }
        }
    }
};

struct search_stack {
    int ply;
    move m;
    int static_eval;
    int pv_length;
    std::array<move, MAX_DEPTH> pv_line;

    void reset() {
        ply = 0;
        m = move::none();
        static_eval = VALUE_NONE;
        pv_length = 0;
    }
};

struct engine {
    struct result {
        int depth;
        move m;
        int score;
    };

    board m_board;
    search_stack m_ss[MAX_DEPTH + SS_HEAD];
    timer m_timer;
    int64_t nodes;

    explicit engine(board board) : m_board(board) {}

    int negamax(int alpha, int beta, search_stack *ss) {
        ss->pv_length = 0;
        nodes += 1;
        if (nodes & 4095) {
            m_timer.check();
            if (m_timer.is_stopped())
                return 0;
        }

        // terminal check
        int state = m_board.get_state();
        if (state != NONE) {
            if (state == DRAW)
                return VALUE_DRAW;

            return -INF + ss->ply;
        }

        // tt lookup

        // pruning

        // negamax

        // history update
    }


    result search() {

        // setup search stack
        for (int i = 0; i < SS_HEAD; ++i) {
        }

        for (int i = 0; i < MAX_DEPTH; ++i) {
            m_ss[i + SS_HEAD].reset();
            m_ss[i + SS_HEAD].ply = i;
        }

        result res = {.depth = 0, .m = move::none(), .score = 0};

        for (int depth = 1; depth < MAX_DEPTH; ++depth) {
            int alpha = -INF;
            int beta = INF;

            int window = 100;

            if (depth > 4) {
                alpha = std::max(-INF, res.score - window);
                beta = std::min(INF, res.score + window);
            }

            // asp window search
            while (true) {
                int score = negamax(alpha, beta, &m_ss[SS_HEAD]);
                if (m_timer.is_stopped())
                    break;

                if (score < alpha) {
                    beta = alpha + 1;
                    alpha = std::max(-INF, score - window);
                } else if (score > beta) {
                    alpha = beta - 1;
                    beta = std::min(INF, score + window);
                } else {
                    res.depth = depth;
                    res.score = score;
                    break;
                }
            }

            // update pv
            res.m = m_ss[SS_HEAD].pv_line[0];

            if (m_timer.is_stopped())
                break;

            if (m_timer.is_optimal_stopped())
                break;

            // print stats
            std::cout << "info" << " depth " << depth << " score " << score_to_cp(res.score) << " nodes " << nodes
                      << "\n";
        }

        return res;
    }
};
