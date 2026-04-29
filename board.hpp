#pragma once

#include <cinttypes>
#include "color.hpp"

constexpr int HEIGHT = 12;
constexpr int SIZE = 8;
constexpr int SIDES = 2;
constexpr int DROPS = 4 * 2;
constexpr int DRAW_LENGTH = 200;
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
    // original height
    int height;
    // src square
    int square;
    // move dir
    int dir;


    std::array<int8_t, SIDES * SIDES> old_heights;
    std::array<uint64_t, SIDES> old_occ;

    static move make_drop(int square) {
        return {.expand = false, .height = 0, .square = square, .dir = 0, .old_heights = {}, .old_occ = {}};
    }

    static move make_normal(int square, int height, int dir) {
        return {.expand = false, .height = height, .square = square, .dir = dir, .old_heights = {}, .old_occ = {}};
    }

    static move make_expand(int square, int dir) {
        return {.expand = true, .height = 0, .square = square, .dir = dir, .old_heights = {}, .old_occ = {}};
    }

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
        }
    }

    int type() const {
        if (dir == NONE)
            return PLACE;

        if (expand == 0)
            return NORMAL;

        return EXPAND;
    }

    int to() const { return square + dir; }

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
        }
    }
};

struct piece {
    int side;
    int8_t height;
};


struct board {
    std::array<int8_t, SIZE * SIZE> heights;
    std::array<uint64_t, SIDES> occ;

    int side2move;
    int moves;


    static board startpos() { return {.heights = {}, .occ = {}, .side2move = WHITE, .moves = 0}; }

    void make_move(move &m) {
        switch (m.type()) {
            case move::NORMAL: {
                heights[m.to()] += heights[m.square];
                heights[m.square] = 0;
                occ[side2move] ^= (1ull << m.square);
                occ[side2move] |= (1ull << m.to());
                break;
            }
            case move::PLACE: {
                heights[m.square] += PLACE_SIZE;
                occ[side2move] |= (1ull << m.square);
                break;
            }
            case move::EXPAND: {
                // copy move
                std::memcpy(&m.old_heights, &heights, sizeof(heights));
                std::memcpy(&m.old_occ, &occ, sizeof(occ));


                std::array<int, SIZE> shifts{};

                // TODO: optimize power here
                int limit = m.edge_distance();
                int power = (int) heights[m.square] - 1;
                int before = power;
                for (int step = 1; step <= limit; ++step) {
                    int sq = m.square + m.dir * step;
                    if ((occ[0] | occ[1]) & (1ull << sq)) {
                        shifts[step] = before;
                    } else {
                        before -= 1;
                    }
                }

                for (int step = limit; step >= 1; --step) {
                    int sq = m.square + m.dir * step;

                    int shift = shifts[step];
                    if (shift > 0) {
                        // try shift
                        if (step + shift <= limit) {
                            int shift_sq = m.square + m.dir * (step + shift);

                            heights[shift_sq] = heights[sq];
                            if (occ[0] & (1ull << sq)) {
                                occ[0] |= (1ull << shift_sq);
                            } else {
                                occ[1] |= (1ull << shift_sq);
                            }
                        }

                        // clear current
                        heights[sq] = 0;
                        occ[0] &= ~(1ull << sq);
                        occ[1] &= ~(1ull << sq);
                    }

                    if (step <= power) {
                        heights[sq] = 1;
                        occ[side2move] |= (1ull << sq);
                        occ[~side2move] &= ~(1ull << sq);
                    }
                }

                heights[m.square] = 1;

                break;
            }
        }

        side2move ^= 1;
        moves += 1;
    }

    void unmake_move(const move &m) {}

    int get_state() const {
        if (moves < DROPS)
            return NONE;

        if (occ[side2move] == 0)
            return ~side2move;

        if (moves >= DRAW_LENGTH)
            return DRAW;

        return NONE;
    }

    piece at(int index) const {
        if (occ[0] & (1ull << index))
            return {.side = 0, .height = heights[index]};

        if (occ[1] & (1ull << index))
            return {.side = 1, .height = heights[index]};

        return {.side = NONE, .height = 0};
    }

    bool is_drop() const { return moves < DROPS; }

    void display() const {
        for (int i = 0; i < SIZE * SIZE; ++i) {
            piece p = at(i);
            switch (p.side) {
                case WHITE: {
                    std::cout << "|" << RED << (std::to_string(p.height)) << RESET;
                    break;
                }
                case BLACK: {
                    std::cout << "|" << BLUE << (std::to_string(p.height)) << RESET;
                    break;
                }
                case NONE: {
                    std::cout << "| ";
                    break;
                }
            }

            if (i % SIZE == (SIZE - 1))
                std::cout << "|\n";
        }

        std::cout << "state " << get_state() << "\n";
        std::cout << "side2move " << side2move << "\n";
        std::cout << "is_drop " << (moves < DROPS ? "true" : "false") << "\n";
    }
};

struct movegen {
    const board &m_board;

    std::vector<move> get_drops() {
        uint64_t occ = ~m_board.occ[m_board.side2move ^ 1];
        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            moves.push_back(move::make_drop(idx));
        }

        return moves;
    }

    std::vector<move> get_normals() {
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            constexpr std::array<int, 4> dirs{move::UP, move::DOWN, move::LEFT, move::RIGHT};
            for (int dir: dirs) {
                if (move::valid_normal(idx, dir)) {
                    moves.push_back(move::make_normal(idx, m_board.heights[idx], dir));
                }
            }
        }
        return moves;
    }


    std::vector<move> get_expands() {
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ) {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            if (m_board.heights[idx] == 1) {
                continue;
            }

            constexpr std::array<int, 4> dirs{move::UP, move::DOWN, move::LEFT, move::RIGHT};
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
};
