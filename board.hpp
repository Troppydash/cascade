#pragma once

#include <array>
#include <cassert>
#include <format>
#include <iostream>
#include <random>
#include <vector>
#include "color.hpp"

constexpr int HEIGHT = 12;
constexpr int SIZE = 8;
constexpr int SIDES = 2;
constexpr int DROPS = 4 * 2;
constexpr int DRAW_LENGTH = 300;
constexpr int PLACE_SIZE = 3;

constexpr int WHITE = 0;
constexpr int BLACK = 1;
constexpr int DRAW = 2;
constexpr int NONE = 3;

struct move {
    constexpr static int UP = -SIZE;
    constexpr static int DOWN = SIZE;
    constexpr static int LEFT = -1;
    constexpr static int RIGHT = 1;
    constexpr static int NONE = 0;

    constexpr static int NORMAL = 0;
    constexpr static int EXPAND = 1;
    constexpr static int PLACE = 2;

    // expand
    bool expand;
    // src square
    int square;
    // move dir
    int dir;

    int score = 0;

    static move make_drop(int square) {
        assert(square >= 0 && square < 64);
        return {.expand = false, .square = square, .dir = NONE};
    }

    static move make_normal(int square, int dir) {
        assert(square >= 0 && square < 64);
        return {.expand = false, .square = square, .dir = dir};
    }

    static move make_expand(int square, int dir) {
        assert(square >= 0 && square < 64);
        return {.expand = true, .square = square, .dir = dir};
    }

    constexpr bool operator==(const move &other) const {
        return expand == other.expand && square == other.square && dir == other.dir;
    }

    constexpr static move none() { return {.expand = false, .square = 64, .dir = 0}; }

    static bool valid_normal(int square, int dir) {
        int row = square / SIZE;
        int col = square % SIZE;
        switch (dir) {
            case UP:
                return row > 0;
            case DOWN:
                return row < (SIZE - 1);
            case LEFT:
                return col > 0;
            case RIGHT:
                return col < (SIZE - 1);
            default:
                std::cerr << "valid_normal failed\n";
                exit(1);
        }
    }

    int get_dir() const {
        switch (dir) {
            case UP:
                return 0;
            case DOWN:
                return 1;
            case LEFT:
                return 2;
            case RIGHT:
                return 3;
            default:
                std::cerr << "valid_normal failed\n";
                exit(1);
        }
    }

    static int of_dir(int encoded) {
        constexpr std::array<int, 4> mapping = {UP, DOWN, LEFT, RIGHT};
        return mapping[encoded];
    }

    bool is_none() const { return (square == 64); }

    int type() const {
        if (dir == NONE)
            return PLACE;

        if (expand == 0)
            return NORMAL;

        return EXPAND;
    }

    int to() const {
        assert(square + dir >= 0);
        assert(square + dir < 64);
        return square + dir;
    }

    int row() const { return square / SIZE; }

    int col() const { return square % SIZE; }

    int edge_distance() const {
        switch (dir) {
            case UP:
                return row();
            case DOWN:
                return SIZE - row() - 1;
            case LEFT:
                return col();
            case RIGHT:
                return SIZE - col() - 1;
            default:
                std::cerr << "edge_distance failed\n";
                exit(1);
        }
    }

    void display() const {
        switch (type()) {
            case PLACE:
                std::cout << "Place (" << row() + 1 << "/" << col() + 1 << ")";
                break;
            case NORMAL:
                std::cout << "MOVE (" << row() + 1 << "/" << col() + 1 << "," << dir << ")";
                break;
            case EXPAND:
                std::cout << "EXPAND (" << row() + 1 << "/" << col() + 1 << "," << dir << ")";
                break;
            default:
                std::cerr << "display failed\n";
                exit(1);
        }
    }

    std::string str() const {
        switch (type()) {
            case PLACE:
                return std::format("P{}{}", row(), col());
            case NORMAL:
                return std::format("N{}{}{}", row(), col(), get_dir());
            case EXPAND: {
                return std::format("E{}{}{}", row(), col(), get_dir());
            }
            default:
                std::cerr << "display failed\n";
                exit(1);
        }
    }

    static int make_sq(int row, int col) { return row * SIZE + col; }

    static move of_string(const std::string &str) {
        if (str[0] == 'P') {
            return make_drop(make_sq(str[1] - '0', str[2] - '0'));
        }

        if (str[0] == 'N') {
            return make_normal(make_sq(str[1] - '0', str[2] - '0'), of_dir(str[3] - '0'));
        }

        if (str[0] == 'E') {
            return make_expand(make_sq(str[1] - '0', str[2] - '0'), of_dir(str[3] - '0'));
        }

        std::cerr << "invalid move\n";
        exit(0);
    }
};

struct piece {
    int side;
    int8_t height;
};

struct zobrist {
    uint64_t pst[12][64][2];
    uint64_t stage;
    uint64_t side2move;

    // Delete copy constructor and assignment operator to prevent duplicates
    zobrist(const zobrist &) = delete;
    zobrist &operator=(const zobrist &) = delete;

    // Static access method
    static zobrist &get() {
        static zobrist instance;
        return instance;
    }

private:
    explicit zobrist() { init_keys(); }

    void init_keys() {
        std::mt19937_64 gen(42); // Seeded for reproducibility
        std::uniform_int_distribution<uint64_t> dist;

        for (int p = 0; p < 12; ++p)
            for (int s = 0; s < 64; ++s)
                for (int t = 0; t < 2; ++t)
                    pst[p][s][t] = dist(gen);

        stage = dist(gen);
        side2move = dist(gen);
    }
};

struct board {
    std::array<int8_t, SIZE * SIZE> heights;
    std::array<uint64_t, SIDES> occ;

    int side2move;
    int moves;

    struct history {
        std::array<int8_t, SIZE * SIZE> heights;
        std::array<uint64_t, SIDES> occ;
        uint64_t hash;
    };

    int past_length;
    std::array<history, DRAW_LENGTH + 1> past;

    static board startpos() {
        board start = {.heights = {}, .occ = {}, .side2move = WHITE, .moves = 0, .past_length = 0, .past = {}};

        // create first history
        start.past[0].heights = start.heights;
        start.past[0].occ = start.occ;
        const zobrist &zob = zobrist::get();
        start.past[0].hash = zob.side2move;

        return start;
    }

    void make_move(const move &m) {
        const zobrist &zob = zobrist::get();
        uint64_t hash = past[past_length].hash;
        hash ^= zob.side2move;
        if (moves + 1 == DROPS)
            hash ^= zob.stage;

        if (!m.is_none()) {
            switch (m.type()) {
                case move::NORMAL: {
                    past[past_length].heights[m.square] = heights[m.square];
                    past[past_length].heights[m.to()] = heights[m.to()];
                    past[past_length].occ = occ;

                    assert(heights[m.square] > 0);
                    assert(occ[side2move] & (1ull << m.square));

                    if (occ[side2move ^ 1] & (1ull << m.to())) {
                        assert(heights[m.to()] > 0);
                        // capture
                        hash ^= zob.pst[heights[m.square]][m.to()][side2move];
                        hash ^= zob.pst[heights[m.to()]][m.to()][side2move ^ 1];

                        heights[m.to()] = heights[m.square];
                        occ[side2move] |= (1ull << m.to());
                        occ[side2move ^ 1] ^= (1ull << m.to());
                    } else if (occ[side2move] & (1ull << m.to())) {
                        assert(heights[m.to()] > 0);
                        // stack
                        hash ^= zob.pst[heights[m.to()]][m.to()][side2move];
                        heights[m.to()] += heights[m.square];
                        hash ^= zob.pst[heights[m.to()]][m.to()][side2move];
                    } else {
                        // normal
                        hash ^= zob.pst[heights[m.square]][m.to()][side2move];

                        heights[m.to()] = heights[m.square];
                        occ[side2move] |= (1ull << m.to());
                    }

                    hash ^= zob.pst[heights[m.square]][m.square][side2move];
                    heights[m.square] = 0;
                    occ[side2move] ^= (1ull << m.square);


                    break;
                }
                case move::PLACE: {
                    occ[side2move] |= (1ull << m.square);
                    heights[m.square] = PLACE_SIZE;
                    hash ^= zob.pst[heights[m.square]][m.square][side2move];
                    break;
                }
                case move::EXPAND: {
                    // save
                    past[past_length].heights = heights;
                    past[past_length].occ = occ;

                    std::array<int, SIZE> shifts{};

                    int limit = m.edge_distance();
                    int power = (int) heights[m.square];
                    int before = power;
                    for (int step = 1; step <= limit; ++step) {
                        int sq = m.square + m.dir * step;
                        assert(sq < 64 && sq >= 0);
                        if ((occ[0] | occ[1]) & (1ull << sq)) {
                            shifts[step] = before;
                        } else {
                            shifts[step] = 0;

                            before -= 1;
                            if (before == 0) {
                                limit = step;
                                break;
                            }
                        }
                    }

                    for (int step = limit; step >= 1; --step) {
                        int sq = m.square + m.dir * step;
                        auto sq_data = at(sq);

                        int shift = shifts[step];
                        if (shift > 0 && sq_data.side != NONE) {
                            // try shift
                            if (step + shift <= limit) {
                                int shift_sq = m.square + m.dir * (step + shift);

                                auto shift_sq_data = at(shift_sq);
                                if (shift_sq_data.side == NONE) {
                                    hash ^= zob.pst[sq_data.height][shift_sq][sq_data.side];

                                    heights[shift_sq] = heights[sq];
                                    occ[sq_data.side] |= (1ull << shift_sq);
                                } else {
                                    hash ^= zob.pst[shift_sq_data.height][shift_sq][shift_sq_data.side];
                                    hash ^= zob.pst[sq_data.height][shift_sq][sq_data.side];

                                    heights[shift_sq] = heights[sq];
                                    occ[shift_sq_data.side] ^= (1ull << shift_sq);
                                    occ[sq_data.side] |= (1ull << shift_sq);
                                }
                            }

                            // clear current
                            hash ^= zob.pst[sq_data.height][sq][sq_data.side];

                            heights[sq] = 0;
                            occ[sq_data.side] ^= (1ull << sq);
                        }

                        // this can only happen if underlying square is empty
                        if (step <= power) {
                            assert(at(sq).side == NONE);

                            hash ^= zob.pst[1][sq][side2move];

                            heights[sq] = 1;
                            occ[side2move] |= (1ull << sq);
                        }
                    }

                    hash ^= zob.pst[heights[m.square]][m.square][side2move];
                    heights[m.square] = 0;
                    occ[side2move] ^= (1ull << m.square);

                    break;
                }
                default:
                    std::cerr << "make_move failed\n";
                    exit(1);
            }
        }

        past_length += 1;
        past[past_length].hash = hash;
        side2move ^= 1;
        moves += 1;
    }

    void unmake_move(const move &m) {
        assert(past_length >= 1);

        past_length -= 1;
        moves -= 1;
        side2move ^= 1;

        if (!m.is_none()) {

            switch (m.type()) {
                case move::NORMAL: {
                    heights[m.square] = past[past_length].heights[m.square];
                    heights[m.to()] = past[past_length].heights[m.to()];
                    occ = past[past_length].occ;
                    break;
                }
                case move::PLACE: {
                    heights[m.square] = 0;
                    occ[side2move] ^= (1ull << m.square);
                    break;
                }
                case move::EXPAND: {
                    heights = past[past_length].heights;
                    occ = past[past_length].occ;

                    break;
                }
                default:
                    std::cerr << "unmake_move failed\n";
                    exit(1);
            }
        }
    }

    uint64_t get_hash() const { return past[past_length].hash; }

    int get_draw_state() const {
        // count pieces
        int p0 = 0;
        int p1 = 0;
        uint64_t mask = occ[0];
        while (mask) {
            int i = __builtin_ctzll(mask);
            mask ^= (1ull << i);

            p0 += heights[i];
        }

        mask = occ[1];
        while (mask) {
            int i = __builtin_ctzll(mask);
            mask ^= (1ull << i);

            p1 += heights[i];
        }

        if (p0 > p1)
            return WHITE;
        if (p0 < p1)
            return BLACK;

        return DRAW;
    }

    int get_state(bool skip_rep = false) const {
        if (!skip_rep && is_repetition(0))
            return DRAW;

        if (moves < DROPS)
            return NONE;

        if (occ[side2move] == 0)
            return side2move ^ 1;

        if (moves >= DRAW_LENGTH) {
            return get_draw_state();
        }

        return NONE;
    }

    bool is_lost() const {
        if (!is_drop() && std::popcount(occ[side2move]) == 1) {
            int idx = __builtin_ctzll(occ[side2move]);
            if (heights[idx] == 1) {

                // check if opp has height 2
                uint64_t mask = occ[side2move ^ 1];
                while (mask) {
                    int i = __builtin_ctzll(mask);
                    mask ^= (1ull << i);
                    if (heights[i] >= 2)
                        return true;
                }
            }
        }
        return false;
    }

    int is_repetition(int ply) const {
        int count = 0;
        for (int i = 2; true; i += 2) {
            if (past_length - i < 0)
                break;

            if (past[past_length].hash == past[past_length - i].hash) {
                if (i <= ply)
                    return 1;

                count += 1;
                if (count > 1)
                    return 2;
            }
        }

        return 0;
    }

    bool is_capture(const move &m) const {
        assert(m.type() == move::NORMAL);
        return at(m.to()).side == (side2move ^ 1);
    }

    piece at(int index) const {
        assert(index >= 0 && index < 64);
        if (occ[0] & (1ull << index))
            return {.side = 0, .height = heights[index]};

        if (occ[1] & (1ull << index))
            return {.side = 1, .height = heights[index]};

        return {.side = NONE, .height = 0};
    }

    uint64_t occ_with_height(int side, int low, int high) const {
        uint64_t mask = occ[side];
        uint64_t out = 0;
        while (mask) {
            int i = __builtin_ctzll(mask);
            mask ^= (1ull << i);

            if (heights[i] >= low && heights[i] <= high)
                out |= (1ull << i);
        }

        return out;
    }

    bool is_drop() const { return moves < DROPS; }

    bool is_legal(const move &m) const {
        assert(!m.is_none());
        if (is_drop() && m.type() != move::PLACE)
            return false;

        if (!is_drop() && m.type() == move::PLACE)
            return false;

        switch (m.type()) {
            case move::NORMAL: {
                auto start = at(m.square);
                auto end = at(m.to());
                if (start.side != side2move)
                    return false;

                if (end.side != side2move && start.height < end.height)
                    return false;

                return true;
            }
            case move::PLACE: {
                auto start = at(m.square);
                if (start.side == (side2move ^ 1))
                    return false;

                return true;
            }
            case move::EXPAND: {
                auto start = at(m.square);
                if (start.side != side2move)
                    return false;

                if (start.height == 1)
                    return false;

                return true;
            }
        }

        exit(0);
    }


    void display() const {
        auto to_string = [](int height) -> char {
            if (height == 12) {
                return 'C';
            }
            if (height == 11) {
                return 'B';
            }
            if (height == 10) {
                return 'A';
            }
            return '0' + height;
        };

        for (int i = 0; i < SIZE * SIZE; ++i) {
            piece p = at(i);
            switch (p.side) {
                case WHITE: {
                    std::cout << "|" << RED << (to_string(p.height)) << RESET;
                    break;
                }
                case BLACK: {
                    std::cout << "|" << BLUE << (to_string(p.height)) << RESET;
                    break;
                }
                case NONE: {
                    std::cout << "| ";
                    break;
                }
                default:
                    std::cerr << "display failed\n";
                    exit(1);
            }

            if (i % SIZE == (SIZE - 1))
                std::cout << "|\n";
        }

        std::cout << "state " << get_state() << "\n";
        std::cout << "side2move " << side2move << "\n";
        std::cout << "moves " << moves << "\n";
        std::cout << "is_drop " << (moves < DROPS ? "true" : "false") << "\n";
    }
};

struct movegen {
    const board &m_board;

    static uint64_t dilate(uint64_t M) {
        uint64_t up = M >> 8;
        uint64_t down = M << 8;
        // Masking ensures bits on the left edge don't wrap to the right of the row above
        uint64_t left = (M & 0xFEFEFEFEFEFEFEFEULL) >> 1;
        uint64_t right = (M & 0x7F7F7F7F7F7F7F7FULL) << 1;

        // Combine them all with OR
        return M | up | down | left | right;
    }

    std::vector<move> get_drops() {
        // expand on opponent
        uint64_t opp_occ = m_board.occ[m_board.side2move ^ 1];
        uint64_t occ = ~dilate(opp_occ) - m_board.occ[m_board.side2move];

        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            moves.push_back(move::make_drop(idx));
        }

        return moves;
    }

    std::vector<move> get_normals() {
        constexpr std::array<int, 4> dirs{move::UP, move::DOWN, move::LEFT, move::RIGHT};

        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            for (int dir: dirs) {
                if (move::valid_normal(idx, dir)) {
                    if (!(m_board.occ[m_board.side2move ^ 1] & (1ull << (idx + dir))) ||
                        m_board.heights[idx] >= m_board.heights[idx + dir])
                        moves.push_back(move::make_normal(idx, dir));
                }
            }
        }
        return moves;
    }


    std::vector<move> get_expands() {
        constexpr std::array<int, 4> dirs{move::UP, move::DOWN, move::LEFT, move::RIGHT};

        std::vector<move> moves{};
        uint64_t occ = m_board.occ[m_board.side2move];
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            if (m_board.heights[idx] == 1) {
                continue;
            }

            for (int dir: dirs) {
                moves.push_back(move::make_expand(idx, dir));
            }
        }
        return moves;
    }

    std::vector<move> get_valids() {
        if (m_board.is_drop()) {
            return get_drops();
        }

        auto expands = get_expands();
        auto normals = get_normals();
        expands.insert(expands.end(), std::make_move_iterator(normals.begin()), std::make_move_iterator(normals.end()));

        return expands;
    }

    std::vector<move> get_captures() {
        constexpr std::array<int, 4> dirs{move::UP, move::DOWN, move::LEFT, move::RIGHT};

        // expands, captures/stacks
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            for (int dir: dirs) {
                if (m_board.heights[idx] > 1) {
                    moves.push_back(move::make_expand(idx, dir));
                }
                if (move::valid_normal(idx, dir)) {
                    bool is_capture = (m_board.occ[m_board.side2move ^ 1] & (1ull << (idx + dir))) &&
                                      m_board.heights[idx] >= m_board.heights[idx + dir];
                    bool is_stack = m_board.occ[m_board.side2move] & (1ull << (idx + dir));
                    if (is_capture || is_stack)
                        moves.push_back(move::make_normal(idx, dir));
                }
            }
        }

        return moves;
    }

    std::vector<move> get_quiets() {
        constexpr std::array<int, 4> dirs{move::UP, move::DOWN, move::LEFT, move::RIGHT};

        // non-captures
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            for (int dir: dirs) {
                if (move::valid_normal(idx, dir)) {
                    if (!((m_board.occ[0] | m_board.occ[1]) & (1ull << (idx + dir))))
                        moves.push_back(move::make_normal(idx, dir));
                }
            }
        }

        return moves;
    }

    static board create_board(const std::vector<move> &moves) {
        board startpos = board::startpos();
        for (auto &m: moves)
            startpos.make_move(m);

        return startpos;
    }
};
