#pragma once


#include <chrono>
#include <format>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include "board.hpp"

constexpr int MAX_DEPTH = 128;
constexpr int SS_HEAD = 10;

constexpr int INF = 32000;
constexpr int CHECKMATE = INF - MAX_DEPTH;
constexpr int MAX_EVAL = CHECKMATE - 10;
constexpr int VALUE_DRAW = 0;
constexpr int VALUE_NONE = INF + 1;
constexpr int QDEPTH = 0;
constexpr int UNSEARCHED_DEPTH = -19;
constexpr int UNINIT_DEPTH = -20;
constexpr int NO_FLAG = 0;
constexpr int ALPHA_FLAG = 1;
constexpr int BETA_FLAG = 2;
constexpr int EXACT_FLAG = 3;


constexpr bool IS_VALID(int value) { return value != VALUE_NONE; }

constexpr int MATED_IN(int ply) { return -INF + ply; }

constexpr int MATE_IN(int ply) { return INF - ply; }

constexpr bool IS_WIN(int value) { return value > CHECKMATE; }

constexpr bool IS_LOSS(int value) { return value < -CHECKMATE; }

constexpr bool IS_DECISIVE(int value) { return IS_WIN(value) || IS_LOSS(value); }


std::string score_to_cp(int score) {
    if (score > CHECKMATE)
        return std::format("mate {}", INF - score);
    if (score < -CHECKMATE)
        return std::format("mate {}", -INF - score);

    return std::format("cp {}", score);
}

namespace param {
    constexpr int EXPAND_QSEARCH_COST = 200;
    constexpr int EXPAND_COST = 100;
}; // namespace param

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

        stopped = (now() >= (base + limit));
    }

    bool is_stopped() const { return stopped; }

    bool is_optimal_stopped() const { return now() >= (base + optimal); }


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
            int8_t flag;
            bool pv;
        };

        uint64_t hash;
        int static_score;
        int score;
        int depth;
        move m;
        int8_t flag;
        bool pv;

        void reset() {
            static_score = VALUE_NONE;
            score = VALUE_NONE;
            depth = UNINIT_DEPTH;
            m = move::none();
            flag = NO_FLAG;
        }

        data get(uint64_t hash, int ply, int depth, int alpha, int beta) {
            if (this->hash == hash) {
                int adjusted_score = VALUE_NONE;
                bool can_use = false;

                if (IS_VALID(this->score)) {
                    adjusted_score = this->score;

                    if (adjusted_score > CHECKMATE)
                        adjusted_score -= ply;
                    if (adjusted_score < -CHECKMATE)
                        adjusted_score += ply;
                }

                if (this->depth >= depth && IS_VALID(this->score)) {
                    if (this->flag == EXACT_FLAG)
                        can_use = true;
                    else if (this->flag == ALPHA_FLAG && adjusted_score <= alpha)
                        can_use = true;
                    else if (this->flag == BETA_FLAG && adjusted_score >= beta)
                        can_use = true;
                }

                return {
                        .hit = true,
                        .can_use = can_use,
                        .static_score = this->static_score,
                        .score = adjusted_score,
                        .depth = this->depth,
                        .m = this->m,
                        .flag = this->flag,
                        .pv = this->pv,
                };
            }

            return {.hit = false,
                    .can_use = false,
                    .static_score = VALUE_NONE,
                    .score = VALUE_NONE,
                    .depth = UNINIT_DEPTH,
                    .m = move::none(),
                    .flag = NO_FLAG,
                    .pv = false};
        }

        void set(uint64_t hash, int flag, int score, int ply, int depth, move m, int static_score, bool pv) {
            if (!m.is_none() || this->hash != hash) {
                this->m = m;
            }

            if (flag == EXACT_FLAG || this->hash != hash || (depth + 5 + pv) > this->depth) {
                this->hash = hash;
                this->depth = depth;
                this->static_score = static_score;

                if (IS_VALID(score)) {
                    if (score > CHECKMATE)
                        score += ply;
                    if (score < -CHECKMATE)
                        score -= ply;
                }
                this->score = score;
                this->flag = flag;
                this->pv = pv;
            }
        }
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

template<typename I, I LIMIT>
struct history_entry {
    I value = 0;

    I get_value() const { return value; }

    void add_bonus(int bonus) {
        I clamped_bonus = std::clamp(bonus, -LIMIT, LIMIT);
        value += clamped_bonus - static_cast<int32_t>(value) * std::abs(clamped_bonus) / LIMIT;
    }
};

constexpr int LOW_PLY = 3;

struct heuristics {
    int lmr[64][64];

    history_entry<int, 20000> drop_history[13][64];
    history_entry<int, 20000> main_history[64][64];
    history_entry<int, 20000> capture_history[13][64];
    history_entry<int, 20000> stack_history[13][64];
    history_entry<int, 20000> expand_history[13][64][4];

    history_entry<int, 20000> low_ply_history[LOW_PLY][64][64];

    move counter_move[13][64];

    std::array<move, 2> killers[MAX_DEPTH];

    explicit heuristics() {
        // set lmr
        for (int depth = 1; depth < 64; ++depth)
            for (int move = 1; move < 64; ++move)
                lmr[depth][move] = std::floor(0.99 + std::log(depth) * std::log(move) / 3.14);

        for (auto &m: killers)
            m[0] = m[1] = move::none();

        for (auto &a: counter_move)
            for (auto &b: a)
                b = move::none();
    }

    [[nodiscard]] int get_lmr(int depth, int move) const { return lmr[std::min(63, depth)][std::min(63, move)]; }
};

// TODO: make this inc
struct evaluator {
    // note that 100 is 1 piece
    int pst[64][13];

    explicit evaluator() {
        const std::array<int, 13> heights = {0, -10, 0, 10, 30, 40, 30, 10, 0, -40, -50, -60, -70};

        // create pst
        for (int i = 0; i < 64; ++i) {
            int row = i / 8;
            int col = i % 8;
            int sq = 3 - std::abs(row - 3) + 3 - std::abs(col - 3);
            int sq_value = sq * 5;

            for (int j = 0; j < 13; ++j) {
                int height_value = j * 100 + heights[j];
                pst[i][j] = sq_value + height_value;
            }
        }
    }

    int evaluate(const board &board, bool draw) {
        assert(board.get_state(true) == NONE);

        int total = 0;

        uint64_t occ = board.occ[0] | board.occ[1];
        while (occ) {
            int i = __builtin_ctzll(occ);
            occ ^= (1ull << i);

            if (board.occ[board.side2move] & (1ull << i)) {
                total += pst[i][board.heights[i]];
            } else {
                total -= pst[i][board.heights[i]];
            }
        }

        // drop tempo
        if (board.is_drop()) {
            total += 10;
        } else {
            total += 20;
        }

        if (draw) {
            // draw contempt
            int dist = board.moves;
            int state = board.get_draw_state();
            int contempt = (state == DRAW ? 0 : state == board.side2move ? 1 : -1);
            total += contempt * dist;
        }
        return total;
    }
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

        QPV,
        QCAPTURE_INIT,
        QCAPTURE,

        DONE
    };

    int m_stage;
    move m_pv;
    const board &m_board;
    const heuristics &m_heur;
    const evaluator &m_eval;
    int m_ply;
    bool m_skip_quiet;

    const move &m_prev_move;

    int move_ptr;
    int bad_moves;
    std::vector<move> moves;

    // negamax, qsearch
    explicit movepick(const move &pv, const board &board, const move &prev_move, int ply, const heuristics &heur,
                      const evaluator &eval, stage st = PV) :
        m_pv(pv), m_board(board), m_heur(heur), m_eval(eval), m_prev_move{prev_move}, m_ply(ply), move_ptr{0}, moves{},
        bad_moves{}, m_skip_quiet{false} {
        if (m_board.is_drop())
            m_stage = DROP_PV;
        else
            m_stage = st;
    }


    int eval_expand_pushoffs(const move &m) const {
        assert(m.type() == move::EXPAND);
        int score = -m_eval.pst[m.square][m_board.at(m.square).height];

        int limit = m.edge_distance();
        int power = (int) m_board.heights[m.square] - 1;
        int before = power;
        for (int step = 1; step <= limit; ++step) {
            int sq = m.square + m.dir * step;
            assert(sq < 64 && sq >= 0);
            score += m_eval.pst[sq][1];
            if ((m_board.occ[0] | m_board.occ[1]) & (1ull << sq)) {
                auto p = m_board.at(sq);

                // shift off
                if (step + before > limit) {
                    if (p.side == m_board.side2move) {
                        score -= m_eval.pst[sq][p.height];
                    } else {
                        score += m_eval.pst[sq][p.height];
                    }
                }
            } else {
                before -= 1;
                if (before == 0) {
                    break;
                }
            }
        }

        return score;
    }

    void skip_quiet() { m_skip_quiet = true; }

    move next_move() {
        while (true) {
            switch ((stage) m_stage) {
                case DROP_PV: {
                    m_stage++;
                    if (!m_pv.is_none() && m_board.is_legal(m_pv)) {
                        return m_pv;
                    }
                    break;
                }
                case DROP_INIT: {
                    movegen gen{m_board};
                    moves = gen.get_drops();
                    move_ptr = 0;

                    for (int i = 0; i < moves.size(); ++i) {
                        auto &m = moves[i];
                        m.score = m_heur.drop_history[m_board.heights[m.square]][m.square].get_value();
                    }

                    sort_moves(moves, 0, moves.size());

                    m_stage++;
                    break;
                }

                case DROP_MOVES: {
                    move_ptr = pick_move(moves, move_ptr, moves.size(), [](auto &) { return true; });
                    if (move_ptr < moves.size())
                        return moves[move_ptr++];

                    m_stage = DONE;
                    break;
                }

                case PV: {
                    m_stage++;
                    if (!m_pv.is_none() && m_board.is_legal(m_pv)) {
                        return m_pv;
                    }
                    break;
                }
                case CAPTURE_INIT: {
                    movegen gen{m_board};
                    moves = gen.get_captures();
                    move_ptr = 0;

                    // score moves
                    for (int i = 0; i < moves.size(); ++i) {
                        auto &m = moves[i];

                        if (m == m_pv) {
                            std::swap(m, moves.back());
                            moves.pop_back();
                            i--;
                            continue;
                        }

                        if (m.type() == move::NORMAL) {
                            if (m_board.is_capture(m)) {
                                m.score = m_heur.capture_history[m_board.heights[m.square]][m.to()].get_value() -
                                          m_board.heights[m.square] * 100;
                            } else {
                                m.score = m_heur.stack_history[m_board.heights[m.square]][m.to()].get_value();
                            }
                        } else {
                            m.score = m_heur.expand_history[m_board.heights[m.square]][m.square][m.get_dir()]
                                              .get_value() -
                                      m_board.heights[m.square] * 100;
                        }
                    }

                    sort_moves(moves, 0, moves.size());

                    bad_moves = 0;
                    m_stage++;
                    break;
                }
                case GOOD_CAPTURE: {
                    move_ptr = pick_move(moves, move_ptr, moves.size(), [this](move &m) {
                        if (m.type() == move::EXPAND && this->eval_expand_pushoffs(m) < param::EXPAND_COST) {
                            std::swap(moves[bad_moves++], m);
                            return false;
                        } else {
                            return true;
                        }
                    });
                    if (move_ptr < moves.size())
                        return moves[move_ptr++];

                    m_stage++;
                    break;
                }
                case QUIET_INIT: {
                    move_ptr = moves.size();

                    if (!m_skip_quiet) {
                        movegen gen{m_board};
                        std::vector<move> quiets = gen.get_quiets();
                        moves.insert(moves.end(), std::make_move_iterator(quiets.begin()),
                                     std::make_move_iterator(quiets.end()));

                        move counter_move = move::none();
                        if (!m_prev_move.is_none() && m_prev_move.type() == move::NORMAL) {
                            counter_move = m_heur.counter_move[m_board.heights[m_prev_move.to()]][m_prev_move.to()];
                        }

                        // score moves
                        for (int i = move_ptr; i < moves.size(); ++i) {
                            auto &m = moves[i];

                            if (m == m_pv) {
                                std::swap(m, moves.back());
                                moves.pop_back();
                                i--;
                                continue;
                            }

                            if (m == m_heur.killers[m_ply][0]) {
                                m.score = 32000;
                                continue;
                            }
                            if (m == m_heur.killers[m_ply][1]) {
                                m.score = 31000;
                                continue;
                            }

                            m.score = m_heur.main_history[m.square][m.to()].get_value();
                            if (m_ply < LOW_PLY)
                                m.score +=
                                        2 * m_heur.low_ply_history[m_ply][m.square][m.to()].get_value() / (1 + m_ply);

                            if (m == counter_move)
                                m.score += 20000;

                            m.score = std::clamp(m.score, -30000, 30000);
                        }

                        sort_moves(moves, move_ptr, moves.size());
                    }

                    m_stage++;
                    break;
                }
                case QUIET: {
                    if (!m_skip_quiet) {
                        move_ptr = pick_move(moves, move_ptr, moves.size(), [](auto &) { return true; });
                        if (move_ptr < moves.size())
                            return moves[move_ptr++];
                    }

                    move_ptr = 0;
                    m_stage++;
                    break;
                }
                case BAD_EXPAND: {
                    move_ptr = pick_move(moves, move_ptr, bad_moves, [](auto &m) {
                        assert(m.type() == move::EXPAND);
                        return true;
                    });
                    if (move_ptr < bad_moves)
                        return moves[move_ptr++];

                    m_stage = DONE;
                    break;
                }

                case QPV: {
                    m_stage++;
                    if (!m_pv.is_none() && m_board.is_legal(m_pv)) {
                        return m_pv;
                    }
                    break;
                }
                case QCAPTURE_INIT: {
                    movegen gen{m_board};
                    moves = gen.get_captures();
                    move_ptr = 0;

                    // score moves
                    for (int i = 0; i < moves.size(); ++i) {
                        auto &m = moves[i];

                        if (m == m_pv) {
                            std::swap(m, moves.back());
                            moves.pop_back();
                            i--;
                            continue;
                        }

                        if (m.type() == move::NORMAL) {
                            if (m_board.is_capture(m)) {
                                m.score = m_heur.capture_history[m_board.heights[m.square]][m.to()].get_value() -
                                          m_board.heights[m.square] * 100;
                            } else {
                                m.score = m_heur.stack_history[m_board.heights[m.square]][m.to()].get_value() - 10000;
                            }
                        } else {
                            m.score = m_heur.expand_history[m_board.heights[m.square]][m.square][m.get_dir()]
                                              .get_value() -
                                      m_board.heights[m.square] * 100;
                        }
                    }

                    sort_moves(moves, 0, moves.size());

                    m_stage++;
                    break;
                }
                case QCAPTURE: {
                    move_ptr = pick_move(moves, move_ptr, moves.size(), [this](auto &m) {
                        return m.type() != move::EXPAND || this->eval_expand_pushoffs(m) > param::EXPAND_QSEARCH_COST;
                    });
                    if (move_ptr < moves.size())
                        return moves[move_ptr++];

                    m_stage++;
                    break;
                }

                case DONE: {
                    return move::none();
                }
            }
        }
    }

    template<typename Pred>
    int pick_move(std::vector<move> &moves, const int start, const int end, Pred filter) {
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
            if (moves[i].score >= limit) {
                move temp = moves[i];
                int j = i - 1;
                while (j >= start && moves[j].score < temp.score) {
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
    bool tt_pv;
    std::array<move, MAX_DEPTH> pv_line;

    void reset() {
        ply = 0;
        m = move::none();
        static_eval = VALUE_NONE;
        pv_length = 0;
        tt_pv = false;
    }

    void pv_update(const move &m, search_stack *next) {
        pv_length = next->pv_length + 1;
        pv_line[0] = m;

        for (int i = 0; i < next->pv_length; ++i)
            pv_line[1 + i] = next->pv_line[i];
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
    int sel_depth;

    tt *m_tt;
    std::unique_ptr<heuristics> m_heuristic;
    evaluator m_evaluator;

    explicit engine(const board &board, tt *tt) : m_board(board), m_tt(tt), m_evaluator() {
        m_heuristic = std::make_unique<heuristics>();
    }

    int evaluate(bool draw = false) { return m_evaluator.evaluate(m_board, draw); }

    template<bool is_pv_node>
    int qsearch(int alpha, int beta, int depth, search_stack *ss) {
        assert(alpha < beta);

        ss->pv_length = 0;
        nodes += 1;
        sel_depth = std::max(sel_depth, ss->ply);
        if ((nodes & 4095) == 0) {
            m_timer.check();
        }

        if (m_timer.is_stopped())
            return 0;

        // terminal check
        int state = m_board.get_state(true);
        if (state != NONE) {
            if (state == DRAW)
                return VALUE_DRAW;

            if (state == m_board.side2move)
                return INF - ss->ply;

            return -INF + ss->ply;
        }


        // draw check
        int rep = m_board.is_repetition(ss->ply);
        if (rep) {
            if (rep == 1)
                return evaluate(true);

            return VALUE_DRAW;
        }

        if (ss->ply >= MAX_DEPTH - 5) {
            return evaluate(true);
        }

        if (m_board.is_lost()) {
            return MATED_IN(ss->ply);
        }

        alpha = std::max(alpha, MATED_IN(ss->ply));
        beta = std::min(beta, MATE_IN(ss->ply + 1));
        if (alpha >= beta)
            return alpha;

        // [tt lookup]
        uint64_t key = m_board.get_hash();
        tt::entry *entry = m_tt->get_entry(key);
        tt::entry::data tt_data = entry->get(key, ss->ply, QDEPTH, alpha, beta);

        // early tt cutoff
        if (!is_pv_node && tt_data.can_use)
            return tt_data.score;

        int best_score = -INF;
        int unadjusted_static_score = VALUE_NONE;
        int adjusted_static_score = VALUE_NONE;
        if (tt_data.hit) {
            unadjusted_static_score = tt_data.static_score;
            if (!IS_VALID(unadjusted_static_score))
                unadjusted_static_score = evaluate();

            adjusted_static_score = best_score = unadjusted_static_score;

            bool bound_hit = tt_data.flag == EXACT_FLAG ||
                             (tt_data.flag == BETA_FLAG && tt_data.score > adjusted_static_score) ||
                             (tt_data.flag == ALPHA_FLAG && tt_data.score < adjusted_static_score);
            if (IS_VALID(tt_data.score) && !IS_DECISIVE(tt_data.score) && bound_hit) {
                adjusted_static_score = best_score = tt_data.score;
            }
        } else {
            unadjusted_static_score = evaluate();
            adjusted_static_score = best_score = unadjusted_static_score;

            entry->set(key, NO_FLAG, VALUE_NONE, ss->ply, UNSEARCHED_DEPTH, move::none(), unadjusted_static_score,
                       false);
        }

        // standing pat
        if (best_score >= beta) {
            if (!IS_DECISIVE(best_score)) {
                best_score = (beta + best_score) / 2;
            }

            return best_score;
        }

        if (best_score > alpha)
            alpha = best_score;

        int score;
        move best_move = move::none();
        move m = move::none();
        movepick gen{tt_data.m, m_board, (ss - 1)->m, ss->ply, *m_heuristic, m_evaluator, movepick::stage::QPV};
        int move_count = 0;
        while (!(m = gen.next_move()).is_none()) {
            move_count += 1;

            // fut prune
            if (!IS_LOSS(best_score)) {
                int fut_value = adjusted_static_score + 300;
                if (m.type() == move::NORMAL && m_board.is_capture(m)) {
                    fut_value += m_evaluator.pst[m.to()][m_board.heights[m.to()]];
                } else if (m.type() == move::EXPAND) {
                    fut_value += gen.eval_expand_pushoffs(m);
                } else {
                    fut_value = INF;
                }

                if (fut_value <= alpha) {
                    best_score = std::max(std::min(fut_value, MAX_EVAL), best_score);
                    continue;
                }
            }

            ss->m = m;
            m_board.make_move(m);
            score = -qsearch<is_pv_node>(-beta, -alpha, depth - 1, ss + 1);
            m_board.unmake_move(m);

            if (m_timer.is_stopped())
                return 0;

            if (score > best_score) {
                best_score = score;

                if (score > alpha) {
                    best_move = m;

                    if (score >= beta)
                        break;

                    alpha = score;
                }
            }

            if (!IS_LOSS(best_score)) {
                if (move_count >= 4)
                    break;
            }
        }

        // average out the best score
        if (!IS_DECISIVE(best_score) && best_score > beta) {
            best_score = (beta + best_score) / 2;
        }

        int flag = best_score >= beta ? BETA_FLAG : ALPHA_FLAG;

        entry->set(key, flag, best_score, ss->ply, QDEPTH, best_move, unadjusted_static_score,
                   tt_data.hit && tt_data.pv);

        return best_score;
    }

    template<bool is_pv_node>
    int negamax(int alpha, int beta, int depth, search_stack *ss, bool cut_node) {
        assert(alpha < beta);

        ss->pv_length = 0;
        nodes += 1;
        if ((nodes & 4095) == 0) {
            m_timer.check();
        }

        if (m_timer.is_stopped())
            return 0;

        // terminal check
        int state = m_board.get_state(true);
        if (state != NONE) {
            if (state == DRAW)
                return VALUE_DRAW;

            if (state == m_board.side2move)
                return INF - ss->ply;

            return -INF + ss->ply;
        }


        if (depth <= 0)
            return qsearch<is_pv_node>(alpha, beta, depth, ss);


        bool is_root = ss->ply == 0 && is_pv_node;

        // repetition
        int rep = m_board.is_repetition(ss->ply);
        if (!is_root && rep) {
            if (rep == 1)
                return evaluate(true);

            return VALUE_DRAW;
        }

        if (ss->ply >= MAX_DEPTH - 5)
            return evaluate(true);


        if (!is_root && m_board.is_lost()) {
            return MATED_IN(ss->ply);
        }

        // mate distance pruning
        if (!is_root) {
            alpha = std::max(alpha, MATED_IN(ss->ply));
            beta = std::min(beta, MATE_IN(ss->ply + 1));
            if (alpha >= beta)
                return alpha;
        }


        // tt lookup
        uint64_t key = m_board.get_hash();
        tt::entry *entry = m_tt->get_entry(key);
        tt::entry::data tt_data = entry->get(key, ss->ply, depth, alpha, beta);

        // early tt cutoff
        if (!is_pv_node && tt_data.can_use && (cut_node == (tt_data.score >= beta)) &&
            tt_data.depth >= depth + (tt_data.score >= beta))
            return tt_data.score;

        bool tt_pv = is_pv_node || (tt_data.hit && tt_data.pv);
        ss->tt_pv = tt_pv;
        bool tt_noisy = !tt_data.m.is_none() && (tt_data.m.type() == move::EXPAND ||
                                                 (tt_data.m.type() == move::NORMAL && m_board.is_capture(tt_data.m)));

        int unadjusted_static_score = VALUE_NONE;
        int adjusted_static_score = VALUE_NONE;
        if (tt_data.hit) {
            unadjusted_static_score = tt_data.static_score;
            if (!IS_VALID(unadjusted_static_score))
                unadjusted_static_score = evaluate();

            ss->static_eval = adjusted_static_score = unadjusted_static_score;

            bool bound_hit = tt_data.flag == EXACT_FLAG ||
                             (tt_data.flag == BETA_FLAG && tt_data.score > adjusted_static_score) ||
                             (tt_data.flag == ALPHA_FLAG && tt_data.score < adjusted_static_score);
            if (IS_VALID(tt_data.score) && !IS_DECISIVE(tt_data.score) && bound_hit) {
                adjusted_static_score = tt_data.score;
            }
        } else {
            unadjusted_static_score = evaluate();
            ss->static_eval = adjusted_static_score = unadjusted_static_score;

            entry->set(key, NO_FLAG, VALUE_NONE, ss->ply, UNSEARCHED_DEPTH, move::none(), unadjusted_static_score,
                       ss->tt_pv);
        }

        bool improving = false;
        if (IS_VALID((ss - 2)->static_eval) && IS_VALID(ss->static_eval)) {
            improving = ss->static_eval > (ss - 2)->static_eval;
        } else if (IS_VALID((ss - 4)->static_eval) && IS_VALID(ss->static_eval)) {
            improving = ss->static_eval > (ss - 4)->static_eval;
        }

        // razoring
        if (!is_pv_node && IS_VALID(adjusted_static_score) && !IS_DECISIVE(alpha) &&
            adjusted_static_score < alpha - 300 - 300 * depth * depth) {
            return qsearch<false>(alpha, beta, 0, ss);
        }

        // static null move pruning
        int margin = 100 * depth;
        if (!is_pv_node && IS_VALID(adjusted_static_score) && adjusted_static_score - margin >= beta &&
            !IS_LOSS(beta) && depth <= 10 && !IS_WIN(adjusted_static_score) && (tt_data.m.is_none() || tt_noisy)) {
            return (beta + adjusted_static_score) / 2;
        }

        // nmp
        int count = std::popcount(m_board.occ[m_board.side2move]);
        if (cut_node && !(ss - 1)->m.is_none() && count >= 2 && adjusted_static_score >= beta &&
            IS_VALID(unadjusted_static_score) && !IS_LOSS(beta)) {
            int reduction = 3 + depth / 6;

            int reduced_depth = std::max(0, depth - reduction);

            auto null = move::none();
            ss->m = null;
            m_board.make_move(null);
            int null_score = -negamax<false>(-beta, -beta + 1, reduced_depth, ss + 1, false);
            m_board.unmake_move(null);

            if (m_timer.is_stopped())
                return 0;

            if (null_score >= beta)
                return null_score;
        }

        // iir
        if ((is_pv_node || cut_node) && depth >= (2 + 2 * cut_node) && tt_data.m.is_none()) {
            depth -= 1;
        }

        // TODO: prob cut


        // negamax
        movepick gen{tt_data.m, m_board, (ss - 1)->m, ss->ply, *m_heuristic, m_evaluator, movepick::stage::PV};
        move m;
        int move_count = 0;
        int score;
        int best_score = -INF;
        move best_move = move::none();
        std::vector<move> drop_moves{};
        std::vector<move> capture_moves{};
        std::vector<move> expand_moves{};
        std::vector<move> quiet_moves{};
        while (!(m = gen.next_move()).is_none()) {
            move_count += 1;

            // low depth pruning
            if (!is_root && count >= 2 && !IS_LOSS(best_score)) {
                int lmr_depth = std::clamp(depth - m_heuristic->get_lmr(depth, move_count), 1, depth + 1);

                // late move pruning
                if (move_count >= (3 + depth * depth) / (2 - improving)) {
                    gen.skip_quiet();
                }

                // fut prune
                if (m.type() == move::NORMAL && !m_board.is_capture(m) && lmr_depth <= 6 &&
                    ss->static_eval + 200 + 200 * lmr_depth <= alpha) {
                    gen.skip_quiet();
                    continue;
                }

                // capture fut prune
                bool is_capture = (m.type() == move::NORMAL && m_board.is_capture(m)) || m.type() == move::EXPAND;
                if (is_capture && lmr_depth <= 6) {
                    int additional = 0;
                    if (m.type() == move::NORMAL && m_board.is_capture(m)) {
                        additional = m_evaluator.pst[m.to()][m_board.heights[m.to()]];
                    } else if (m.type() == move::EXPAND) {
                        additional = gen.eval_expand_pushoffs(m);
                    }

                    if (ss->static_eval + 200 + 200 * lmr_depth + additional <= alpha) {
                        continue;
                    }
                }
            }

            int new_depth = depth - 1;
            ss->m = m;
            m_board.make_move(m);

            // lmr
            if (depth >= 2 && move_count > 1 + 2 * is_root) {
                int reduction = m_heuristic->get_lmr(depth, move_count);

                if (cut_node)
                    reduction += 2;

                reduction += !improving;

                reduction -= is_pv_node + tt_pv;

                reduction += tt_noisy;

                reduction -= m.score / 8000;

                int reduced_depth = std::clamp(new_depth - reduction, 1, new_depth + 1);
                score = -negamax<false>(-(alpha + 1), -alpha, reduced_depth, ss + 1, true);
                if (score > alpha && reduced_depth < new_depth) {
                    // new_depth += score > best_score + 60 + new_depth * 2;
                    // new_depth -= score < best_score + 20;

                    if (reduced_depth < new_depth)
                        score = -negamax<false>(-(alpha + 1), -alpha, new_depth, ss + 1, !cut_node);
                }
            } else if (!is_pv_node || move_count > 1) {
                score = -negamax<false>(-(alpha + 1), -alpha, new_depth, ss + 1, !cut_node);
            }

            if (is_pv_node && (move_count == 1 || score > alpha)) {
                score = -negamax<true>(-beta, -alpha, new_depth, ss + 1, false);
            }

            m_board.unmake_move(m);

            if (m_timer.is_stopped())
                return 0;

            if (score > best_score) {
                best_score = score;

                if (score > alpha) {
                    best_move = m;
                    if (is_pv_node)
                        ss->pv_update(best_move, ss + 1);

                    if (score >= beta)
                        break;

                    alpha = score;

                    // if (depth > 4 && depth < 10 && !IS_LOSS(best_score))
                    //     depth -= 1;
                }
            }

            // malus
            switch (m.type()) {
                case move::PLACE: {
                    drop_moves.push_back(m);
                    break;
                }
                case move::EXPAND: {
                    expand_moves.push_back(m);
                    break;
                }
                case move::NORMAL: {
                    if (m_board.at(m.to()).side != m_board.side2move) {
                        capture_moves.push_back(m);
                    } else {
                        quiet_moves.push_back(m);
                    }
                }
            }
        }

        // history update
        if (best_score >= beta) {
            int bonus = 170 * depth - 30;
            int malus = 200 * depth - 30;

            if (m_board.is_drop()) {
                assert(best_move.type() == move::PLACE);
                m_heuristic->drop_history[m_board.heights[best_move.square]][best_move.square].add_bonus(bonus);
                for (auto &m: drop_moves)
                    m_heuristic->drop_history[m_board.heights[m.square]][m.square].add_bonus(-malus);
            } else {
                assert(best_move.type() != move::PLACE);
                switch (best_move.type()) {
                    case move::NORMAL: {
                        if (m_board.at(best_move.to()).side != m_board.side2move) {
                            if (m_board.is_capture(best_move))
                                m_heuristic->capture_history[m_board.heights[best_move.square]][best_move.to()]
                                        .add_bonus(bonus);
                            else
                                m_heuristic->stack_history[m_board.heights[best_move.square]][best_move.to()].add_bonus(
                                        bonus);
                        } else {
                            // quiet cutoff
                            m_heuristic->main_history[best_move.square][best_move.to()].add_bonus(bonus);
                            if (ss->ply < LOW_PLY)
                                m_heuristic->low_ply_history[ss->ply][best_move.square][best_move.to()].add_bonus(
                                        bonus);

                            if (m_heuristic->killers[ss->ply][0] == m) {
                                m_heuristic->killers[ss->ply][1] = m_heuristic->killers[ss->ply][0];
                            }
                            m_heuristic->killers[ss->ply][0] = m;

                            auto prev_move = (ss - 1)->m;
                            if (!prev_move.is_none() && prev_move.type() == move::NORMAL) {
                                m_heuristic->counter_move[m_board.heights[prev_move.to()]][prev_move.to()] = best_move;
                            }

                            for (auto &m: quiet_moves) {
                                m_heuristic->main_history[m_board.heights[m.square]][m.to()].add_bonus(-malus);
                                if (ss->ply < LOW_PLY)
                                    m_heuristic->low_ply_history[ss->ply][m.square][m.to()].add_bonus(-malus);
                            }
                        }

                        break;
                    }
                    case move::EXPAND: {
                        m_heuristic
                                ->expand_history[m_board.heights[best_move.square]][best_move.square]
                                                [best_move.get_dir()]
                                .add_bonus(bonus);

                        break;
                    }
                    default:
                        m_board.display();
                        std::cerr << best_move.str() << std::endl;
                        std::cerr << "wrong best_move type\n";
                        exit(0);
                }

                // always penaltize capture/expands
                for (auto &m: capture_moves) {
                    if (m_board.is_capture(m))
                        m_heuristic->capture_history[m_board.heights[m.square]][m.to()].add_bonus(-malus);
                    else
                        m_heuristic->stack_history[m_board.heights[m.square]][m.to()].add_bonus(-malus);
                }

                for (auto &m: expand_moves)
                    m_heuristic->expand_history[m_board.heights[m.square]][m.square][m.get_dir()].add_bonus(-malus);
            }
        }

        if (best_score <= alpha)
            ss->tt_pv = tt_pv = tt_pv || (ss - 1)->tt_pv;

        int flag = best_score >= beta ? BETA_FLAG : is_pv_node && !best_move.is_none() ? EXACT_FLAG : ALPHA_FLAG;

        entry->set(key, flag, best_score, ss->ply, depth, best_move, unadjusted_static_score, tt_pv);

        return best_score;
    }


    result search(int64_t opt_time, int64_t max_time) {
        nodes = 0;
        sel_depth = 0;

        m_timer.start(opt_time, max_time);

        // setup search stack
        for (int i = 0; i < SS_HEAD; ++i) {
            m_ss[i].reset();
        }

        for (int i = 0; i < MAX_DEPTH; ++i) {
            m_ss[i + SS_HEAD].reset();
            m_ss[i + SS_HEAD].ply = i;
        }

        result res = {.depth = 0, .m = move::none(), .score = 0};

        for (int depth = 1; depth < MAX_DEPTH; ++depth) {
            int alpha = -INF;
            int beta = INF;

            int window = 50 + res.score * res.score / 13000;

            if (depth > 5) {
                alpha = std::max(-INF, res.score - window);
                beta = std::min(INF, res.score + window);
            }

            // asp window search
            while (true) {
                int score = negamax<true>(alpha, beta, depth, &m_ss[SS_HEAD], false);
                m_timer.check();
                if (m_timer.is_stopped())
                    break;

                if (score <= alpha) {
                    beta = alpha + 1;
                    alpha = std::max(-INF, score - window);
                } else if (score >= beta) {
                    alpha = beta - 1;
                    beta = std::min(INF, score + window);
                } else {
                    res.depth = depth;
                    res.score = score;
                    break;
                }
            }

            // update pv
            if (m_ss[SS_HEAD].pv_length > 0)
                res.m = m_ss[SS_HEAD].pv_line[0];

            if (m_timer.is_stopped())
                break;

            if (m_timer.is_optimal_stopped())
                break;

            // print stats
            std::cout << "info" << " depth " << res.depth << " seldepth " << sel_depth << " score "
                      << score_to_cp(res.score) << " nodes " << nodes << " time " << timer::now() - m_timer.base
                      << " pv";
            for (int i = 0; i < m_ss[SS_HEAD].pv_length; ++i)
                std::cout << " " << m_ss[SS_HEAD].pv_line[i].str();
            std::cout << "\n";
        }

        // print stats
        std::cout << "info" << " depth " << res.depth << " seldepth " << sel_depth << " score "
                  << score_to_cp(res.score) << " nodes " << nodes << " time " << timer::now() - m_timer.base << " pv";
        for (int i = 0; i < m_ss[SS_HEAD].pv_length; ++i)
            std::cout << " " << m_ss[SS_HEAD].pv_line[i].str();
        std::cout << "\n";

        return res;
    }
};
