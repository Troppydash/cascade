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


constexpr bool IS_VALID(int value)
{
    return value != VALUE_NONE;
}

constexpr int MATED_IN(int ply)
{
    return -INF + ply;
}

constexpr int MATE_IN(int ply)
{
    return INF - ply;
}

constexpr bool IS_WIN(int value)
{
    return value > CHECKMATE;
}

constexpr bool IS_LOSS(int value)
{
    return value < -CHECKMATE;
}

constexpr bool IS_DECISIVE(int value)
{
    return IS_WIN(value) || IS_LOSS(value);
}


std::string score_to_cp(int score)
{
    if (score > CHECKMATE)
        return std::format("mate {}", INF - score);
    if (score < -CHECKMATE)
        return std::format("mate {}", -INF - score);

    return std::format("cp {}", score);
}

struct timer
{
    int64_t base;
    int64_t optimal;
    int64_t limit;
    bool stopped;

    void start(int64_t optimal, int64_t limit)
    {
        base = now();
        this->optimal = optimal;
        this->limit = limit;
        stopped = false;
    }

    void check()
    {
        if (stopped)
            return;

        stopped = now() >= base + limit;
    }

    bool is_stopped() const { return stopped; }

    bool is_optimal_stopped() const { return now() >= base + optimal; }


    static int64_t now()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};

struct tt
{
    struct entry
    {
        struct data
        {
            bool hit;
            bool can_use;
            int static_score;
            int score;
            int depth;
            move m;
            int flag;
        };

        uint64_t hash;
        int static_score;
        int score;
        int depth;
        move m;
        int flag;

        void reset()
        {
            static_score = VALUE_NONE;
            score = VALUE_NONE;
            depth = UNINIT_DEPTH;
            m = move::none();
            flag = NO_FLAG;
        }

        data get(uint64_t hash, int ply, int depth, int alpha, int beta)
        {
            if (this->hash == hash)
            {
                int adjusted_score = VALUE_NONE;
                bool can_use = false;

                if (IS_VALID(this->score))
                {
                    adjusted_score = this->score;

                    if (adjusted_score > CHECKMATE)
                        adjusted_score -= ply;
                    if (adjusted_score < -CHECKMATE)
                        adjusted_score += ply;
                }

                if (this->depth >= depth && IS_VALID(this->score))
                {
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
                };
            }

            return {
                .hit = false,
                .can_use = false,
                .static_score = VALUE_NONE,
                .score = VALUE_NONE,
                .depth = UNINIT_DEPTH,
                .m = move::none(),
                .flag = NO_FLAG
            };
        }

        void set(uint64_t hash, int flag, int score, int ply, int depth, move m, int static_score)
        {
            if (!m.is_none() || this->hash != hash)
            {
                this->m = m;
            }

            if (flag == EXACT_FLAG || this->hash != hash || (depth + 5) > this->depth)
            {
                this->hash = hash;
                this->depth = depth;
                this->static_score = static_score;

                if (IS_VALID(score))
                {
                    if (score > CHECKMATE)
                        score += ply;
                    if (score < -CHECKMATE)
                        score -= ply;
                }
                this->score = score;
                this->flag = flag;
            }
        }
    };

    int size;
    entry* entries;

    explicit tt(int mb)
    {
        size = mb * 1024 * 1024 / sizeof(entry);
        entries = new entry[size];

        reset();
    }

    void reset()
    {
        for (int i = 0; i < size; ++i)
            entries[i].reset();
    }

    ~tt() { delete[] entries; }

    entry* get_entry(uint64_t hash)
    {
        __int128 index = __int128(hash) * __int128(size) >> 64;
        return &entries[index];
    }
};

template <typename I, I LIMIT> struct history_entry
{
    I value = 0;

    I get_value() const
    {
        return value;
    }

    void add_bonus(int bonus)
    {
        I clamped_bonus = std::clamp(bonus, -LIMIT, LIMIT);
        value += clamped_bonus - static_cast<int32_t>(value) * std::abs(clamped_bonus) / LIMIT;
    }
};

struct heuristics
{
    int lmr[64][64];

    history_entry<int, 20000> drop_history[13][64];
    history_entry<int, 20000> main_history[64][64];
    history_entry<int, 20000> capture_history[13][64];
    history_entry<int, 20000> expand_history[13][64];


    explicit heuristics()
    {
        // set lmr
        for (int depth = 1; depth < 64; ++depth)
            for (int move = 1; move < 64; ++move)
                lmr[depth][move] = std::floor(0.99 + std::log(depth) * std::log(move) / 3.14);
    }

    [[nodiscard]] int get_lmr(int depth, int move) const
    {
        return lmr[std::min(63, depth)][std::min(63, move)];
    }
};

struct movepick
{
    enum stage
    {
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
    const board& m_board;
    const heuristics& m_heur;

    int move_ptr;
    std::vector<move> moves;
    std::vector<move> bad_moves;

    // negamax
    explicit movepick(const move& pv, const board& board, const heuristics& heur) : m_pv(pv), m_board(board), m_heur(heur), move_ptr{ 0 }, moves{}, bad_moves{}
    {
        if (m_board.is_drop())
            m_stage = DROP_PV;
        else
            m_stage = PV;
    }

    move next_move()
    {
        while (true)
        {
            switch ((stage)m_stage)
            {
            case DROP_PV: {
                m_stage++;
                if (!m_pv.is_none() && m_board.is_legal(m_pv))
                {
                    return m_pv;
                }
                break;
            }
            case DROP_INIT: {
                movegen gen{ m_board };
                moves = gen.get_drops();
                move_ptr = 0;

                for (int i = 0; i < moves.size(); ++i)
                {
                    auto& m = moves[i];
                    m.score = m_heur.drop_history[m_board.heights[m.square]][m.square].get_value();
                }

                sort_moves(moves, 0, moves.size());

                m_stage++;
                break;
            }

            case DROP_MOVES: {
                move_ptr = pick_move(moves, move_ptr, moves.size(), [](auto&) { return true; });
                if (move_ptr < moves.size())
                    return moves[move_ptr++];

                m_stage = DONE;
                break;
            }

            case PV: {
                m_stage++;
                if (!m_pv.is_none() && m_board.is_legal(m_pv))
                {
                    return m_pv;
                }
                break;
            }
            case CAPTURE_INIT: {
                movegen gen{ m_board };
                moves = gen.get_captures();
                move_ptr = 0;

                // score moves
                for (int i = 0; i < moves.size(); ++i)
                {
                    auto& m = moves[i];

                    if (m.type() == move::NORMAL)
                    {
                        m.score = m_heur.capture_history[m_board.heights[m.square]][m.to()].get_value() - m_board.heights[m.square] * 2;
                    }
                    else
                    {
                        m.score = m_heur.expand_history[m_board.heights[m.square]][m.to()].get_value() - m_board.heights[m.square] * 2;
                    }

                    std::cout << m.score << "\n";
                }


                sort_moves(moves, 0, moves.size());


                m_stage++;
                break;
            }
            case GOOD_CAPTURE: {
                // TODO: move to bad captures
                move_ptr = pick_move(moves, move_ptr, moves.size(), [](auto&) { return true; });
                if (move_ptr < moves.size())
                    return moves[move_ptr++];

                m_stage++;
                break;
            }
            case QUIET_INIT: {
                movegen gen{ m_board };
                moves = gen.get_quiets();
                move_ptr = 0;

                // score moves
                for (int i = 0; i < moves.size(); ++i)
                {
                    auto& m = moves[i];

                    m.score = m_heur.main_history[m.square][m.to()].get_value();
                }

                sort_moves(moves, 0, moves.size());

                m_stage++;
                break;
            }
            case QUIET: {
                move_ptr = pick_move(moves, move_ptr, moves.size(), [](auto&) { return true; });
                if (move_ptr < moves.size())
                    return moves[move_ptr++];

                move_ptr = 0;
                m_stage++;
                break;
            }
            case BAD_EXPAND: {
                move_ptr = pick_move(bad_moves, move_ptr, bad_moves.size(), [](auto&) { return true; });
                if (move_ptr < bad_moves.size())
                    return bad_moves[move_ptr++];

                m_stage = DONE;
                break;
            }

            case DONE: {
                return move::none();
            }
            }
        }
    }

    template<typename Pred>
    int pick_move(const std::vector<move>& moves, const int start, const int end, Pred filter)
    {
        for (int i = start; i < end; ++i)
        {
            if (!filter(moves[i]))
                continue;

            return i;
        }

        return end;
    }


    static void sort_moves(std::vector<move>& moves, int start, int end,
        int limit = std::numeric_limits<int16_t>::min())
    {
        for (int i = start + 1; i < end; ++i)
        {
            if (moves[i].score >= limit)
            {
                move temp = moves[i];
                int j = i - 1;
                while (j >= start && moves[j].score < temp.score)
                {
                    moves[j + 1] = moves[j];
                    j--;
                }
                moves[j + 1] = temp;
            }
        }
    }
};

struct evaluator
{
    // note that 100 is 1 piece
    int pst[64][13];

    explicit evaluator()
    {
        // create pst
        for (int i = 0; i < 64; ++i)
        {
            int row = i / 8;
            int col = i % 8;
            int sq = 3 - std::abs(row - 3) + 3 - std::abs(col - 3);
            int sq_value = sq * 10;

            for (int j = 0; j < 13; ++j)
            {
                int height_value = j * 20;
                pst[i][j] = 100 + sq_value + height_value;
            }
        }
    }

    int evaluate(const board& board)
    {
        int total = 0;

        uint64_t occ = board.occ[0] | board.occ[1];
        while (occ)
        {
            int i = __builtin_ctzll(occ);
            occ ^= (1ull << i);

            if (board.occ[board.side2move] & (1ull << i))
            {
                total += pst[i][board.heights[i]];
            }
            else
            {
                total -= pst[i][board.heights[i]];
            }
        }

        // drop tempo
        if (board.is_drop())
        {
            total += 10;
        }
        else
        {
            total += 30;
        }

        return total;
    }
};

struct search_stack
{
    int ply;
    move m;
    int static_eval;
    int pv_length;
    std::array<move, MAX_DEPTH> pv_line;

    void reset()
    {
        ply = 0;
        m = move::none();
        static_eval = VALUE_NONE;
        pv_length = 0;
    }

    void pv_update(const move& m, search_stack* next)
    {
        pv_length = next->pv_length + 1;
        pv_line[0] = m;

        for (int i = 0; i < next->pv_length; ++i)
            pv_line[1 + i] = next->pv_line[i];
    }
};

struct engine
{
    struct result
    {
        int depth;
        move m;
        int score;
    };

    board m_board;
    search_stack m_ss[MAX_DEPTH + SS_HEAD];
    timer m_timer;
    int64_t nodes;

    tt m_tt;
    heuristics m_heuristic;
    evaluator m_evaluator;

    explicit engine(const board& board) : m_board(board), m_tt(64), m_heuristic(), m_evaluator() {}

    int evaluate()
    {
        return m_evaluator.evaluate(m_board);
    }

    template <bool is_pv_node>
    int negamax(int alpha, int beta, int depth, search_stack* ss, bool cut_node)
    {
        assert(alpha < beta);

        ss->pv_length = 0;
        nodes += 1;
        if ((nodes & 4095) == 0)
        {
            m_timer.check();
            if (m_timer.is_stopped())
                return 0;
        }

        if (ss->ply >= MAX_DEPTH - 5)
            return evaluate();


        if (depth <= 0)
            return evaluate();

        // terminal check
        int state = m_board.get_state();
        if (state != NONE)
        {
            if (state == DRAW)
                return VALUE_DRAW;

            return -INF + ss->ply;
        }

        bool is_root = ss->ply == 0 && is_pv_node;

        // mate distance pruning
        // if (!is_root)
        // {
        //     alpha = std::max(alpha, MATED_IN(ss->ply));
        //     beta = std::min(beta, MATE_IN(ss->ply + 1));
        //     if (alpha >= beta)
        //         return alpha;
        // }

        // repetition
        if (!is_root && m_board.is_repetition(ss->ply))
            return VALUE_DRAW;


        // tt lookup
        uint64_t key = m_board.get_hash();
        tt::entry* entry = m_tt.get_entry(key);
        tt::entry::data tt_data = entry->get(key, ss->ply, depth, alpha, beta);

        // early tt cutoff
        if (!is_pv_node && tt_data.can_use && (cut_node == (tt_data.score >= beta)) && tt_data.depth >= depth + (tt_data.score >= beta))
            return tt_data.score;

        int unadjusted_static_score = VALUE_NONE;
        int adjusted_static_score = VALUE_NONE;
        if (tt_data.hit)
        {
            unadjusted_static_score = tt_data.static_score;
            if (!IS_VALID(unadjusted_static_score))
                unadjusted_static_score = evaluate();

            adjusted_static_score = unadjusted_static_score;

            bool bound_hit =
                tt_data.flag == EXACT_FLAG ||
                (tt_data.flag == BETA_FLAG && tt_data.score > adjusted_static_score)
                || (tt_data.flag == ALPHA_FLAG && tt_data.score < adjusted_static_score);
            if (IS_VALID(tt_data.score) && !IS_DECISIVE(tt_data.score) && bound_hit)
            {
                adjusted_static_score = tt_data.score;
            }
        }
        else
        {
            unadjusted_static_score = evaluate();
            adjusted_static_score = unadjusted_static_score;

            entry->set(key, NO_FLAG, VALUE_NONE, ss->ply, UNSEARCHED_DEPTH, move::none(), unadjusted_static_score);
        }

        // pruning

        // negamax
        movepick gen{ tt_data.m, m_board, m_heuristic };
        move m;
        int move_count = 0;
        int score;
        int best_score = -INF;
        move best_move = move::none();
        std::vector<move> drop_moves{};
        std::vector<move> capture_moves{};
        std::vector<move> expand_moves{};
        std::vector<move> quiet_moves{};
        while (!(m = gen.next_move()).is_none())
        {
            move_count += 1;

            int new_depth = depth - 1;
            auto h = m_board.get_hash();
            m_board.make_move(m);

            if (depth >= 2 && move_count > 1 + 2 * is_root)
            {
                int reduction = m_heuristic.get_lmr(depth, move_count);

                if (cut_node)
                    reduction += 2;

                reduction -= is_pv_node;

                int reduced_depth = std::clamp(new_depth - reduction, 1, new_depth + 1);
                score = -negamax<false>(-(alpha + 1), -alpha, reduced_depth, ss + 1, true);
                if (score > alpha && reduced_depth < new_depth)
                {
                    if (reduced_depth < new_depth)
                        score = -negamax<false>(-(alpha + 1), -alpha, new_depth, ss + 1, !cut_node);
                }
            }
            else if (!is_pv_node || move_count > 1)
            {
                score = -negamax<false>(-(alpha + 1), -alpha, new_depth, ss + 1, !cut_node);
            }

            if (is_pv_node && (move_count == 1 || score > alpha))
            {
                score = -negamax<true>(-beta, -alpha, new_depth, ss + 1, false);
            }

            m_board.unmake_move(m);
            if (m_board.get_hash() != h)
                std::cout << "before " << h << " after " << m_board.get_hash() << std::endl;

            if (m_timer.is_stopped())
                return 0;

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    best_move = m;
                    if (is_pv_node)
                        ss->pv_update(best_move, ss + 1);

                    if (score >= beta)
                        break;

                    alpha = score;
                }
            }

            // malus
            switch (m.type())
            {
            case move::PLACE: {
                drop_moves.push_back(m);
                break;
            }
            case move::EXPAND: {
                expand_moves.push_back(m);
                break;
            }
            case move::NORMAL: {
                if (m_board.at(m.to()).side != m_board.side2move)
                {
                    capture_moves.push_back(m);
                }
                else
                {
                    quiet_moves.push_back(m);
                }
            }
            }
        }

        // history update
        if (best_score >= beta)
        {
            int bonus = 150 * depth - 30;
            int malus = 150 * depth - 30;

            if (m_board.is_drop())
            {
                m_heuristic.drop_history[m_board.heights[best_move.square]][best_move.square].add_bonus(bonus);
                for (auto& m : drop_moves)
                    m_heuristic.drop_history[m_board.heights[m.square]][m.square].add_bonus(-malus);
            }
            else
            {
                switch (best_move.type())
                {
                case move::NORMAL: {
                    if (m_board.at(best_move.to()).side != m_board.side2move)
                    {
                        m_heuristic.capture_history[m_board.heights[best_move.square]][best_move.to()].add_bonus(bonus);
                    }
                    else
                    {
                        m_heuristic.main_history[best_move.square][best_move.to()].add_bonus(bonus);

                        for (auto& m : quiet_moves)
                            m_heuristic.main_history[m_board.heights[m.square]][m.to()].add_bonus(-malus);
                    }

                    break;
                }
                case move::EXPAND: {
                    m_heuristic.expand_history[m_board.heights[best_move.square]][best_move.to()].add_bonus(bonus);

                    break;
                }
                }

                // always penaltize capture/expands

                for (auto& m : capture_moves)
                    m_heuristic.capture_history[m_board.heights[m.square]][m.to()].add_bonus(-malus);

                for (auto& m : expand_moves)
                    m_heuristic.expand_history[m_board.heights[m.square]][m.to()].add_bonus(-malus);
            }
        }

        int flag = best_score >= beta ? BETA_FLAG : is_pv_node && !best_move.is_none() ? EXACT_FLAG : ALPHA_FLAG;

        entry->set(key, flag, best_score, ss->ply, depth, best_move, unadjusted_static_score);

        return best_score;
    }


    result search(int64_t max_time, int64_t opt_time)
    {
        m_timer.start(opt_time, max_time);

        // setup search stack
        for (int i = 0; i < SS_HEAD; ++i)
        {
            m_ss[i].reset();
        }

        for (int i = 0; i < MAX_DEPTH; ++i)
        {
            m_ss[i + SS_HEAD].reset();
            m_ss[i + SS_HEAD].ply = i;
        }

        result res = { .depth = 0, .m = move::none(), .score = 0 };

        for (int depth = 1; depth < MAX_DEPTH; ++depth)
        {
            int alpha = -INF;
            int beta = INF;

            int window = 300;

            if (depth > 4)
            {
                alpha = std::max(-INF, res.score - window);
                beta = std::min(INF, res.score + window);
            }

            // asp window search
            while (true)
            {
                int score = negamax<true>(alpha, beta, depth, &m_ss[SS_HEAD], false);
                if (m_timer.is_stopped())
                    break;

                if (score <= alpha)
                {
                    beta = alpha + 1;
                    alpha = std::max(-INF, score - window);
                }
                else if (score >= beta)
                {
                    alpha = beta - 1;
                    beta = std::min(INF, score + window);
                }
                else
                {
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
            std::cout << "info" << " depth " << depth << " score " << score_to_cp(res.score) << " nodes " << nodes;
            for (int i = 0; i < m_ss[SS_HEAD].pv_length; ++i)
                std::cout << " " << m_ss[SS_HEAD].pv_line[i].str();
            std::cout << "\n";
        }

        // print stats
        std::cout << "info" << " depth " << res.depth << " score " << score_to_cp(res.score) << " nodes " << nodes;
        for (int i = 0; i < m_ss[SS_HEAD].pv_length; ++i)
            std::cout << " " << m_ss[SS_HEAD].pv_line[i].str();
        std::cout << "\n";

        return res;
    }
};
