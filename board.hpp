#pragma once

#include <cinttypes>

constexpr int HEIGHT = 12;
constexpr int SIZE = 8;
constexpr int SIDES = 2;
constexpr int DROPS = 4 * 2;
constexpr int DRAW_LENGTH = 100;
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
    // src height
    int height;
    // move dir
    int dir;

    std::array<int8_t, SIDES * SIDES> old_heights;
    std::array<uint64_t, SIDES> old_occ;

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
};

struct board {
    std::array<int8_t, SIZE * SIZE> heights;
    std::array<uint64_t, SIDES> occ;

    int drops;
    int side2move;
    int moves;


    static board startpos() { return {.heights = {}, .occ = {}, .drops = 0, .side2move = WHITE, .moves = 0}; }

    void make_move(move &m) {
        switch (m.type()) {
            case move::NORMAL: {
                heights[m.to()] = heights[m.square];
                heights[m.square] = 0;
                occ[side2move] ^= (1ull << m.square) | (1ull << m.to());
                break;
            }
            case move::PLACE: {
                heights[m.square] += PLACE_SIZE;
                occ[side2move] |= (1ull << m.square);

                drops += 1;
                break;
            }
            case move::EXPAND: {
                // TODO: copy here

                int limit = m.edge_distance();
                int shift = (int) heights[m.square] - 1;



                heights[m.square] = 1;

                break;
            }
        }

        side2move ^= 1;
        moves += 1;
    }

    void unmake_move(const move &m) {}

    int get_state() const {
        if (drops < DROPS)
            return NONE;

        if (occ[side2move] == 0)
            return ~side2move;

        if (moves >= DRAW_LENGTH)
            return DRAW;

        return NONE;
    }

    void display() {}
};

struct movegen {};
