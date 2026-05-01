#pragma once

#include <vector>
#include <random>
#include <array>
#include <iostream>
#include <format>
#include <cassert>
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

struct move
{
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

    static move make_drop(int square)
    {
        assert(square >= 0 && square < 64);
        return { .expand = false, .square = square, .dir = NONE };
    }

    static move make_normal(int square, int height, int dir)
    {
        assert(square >= 0 && square < 64);
        return { .expand = false, .square = square, .dir = dir };
    }

    static move make_expand(int square, int dir)
    {
        assert(square >= 0 && square < 64);
        return { .expand = true,  .square = square, .dir = dir };
    }

    static move none()
    {
        return { .expand = false, .square = 64, .dir = 0 };
    }

    static bool valid_normal(int square, int dir)
    {
        int row = square / SIZE;
        int col = square % SIZE;
        switch (dir)
        {
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

    int get_dir() const
    {
        switch (dir)
        {
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

    bool is_none() const { return (square == 64); }

    int type() const
    {
        if (dir == NONE)
            return PLACE;

        if (expand == 0)
            return NORMAL;

        return EXPAND;
    }

    int to() const
    {
        assert(square + dir >= 0);
        assert(square + dir < 64);
        return square + dir;
    }

    int row() const { return square / SIZE; }

    int col() const { return square % SIZE; }

    int edge_distance() const
    {
        switch (dir)
        {
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

    void display() const
    {
        switch (type())
        {
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

    std::string str() const
    {
        switch (type())
        {
        case PLACE:
            return std::format("P{}{}", row() + 1, col() + 1);
        case NORMAL:
            return std::format("N{}{}{}{}", row() + 1, col() + 1, (to() / SIZE) + 1, (to() % SIZE) + 1);
        case EXPAND:
        {
            int value = 0;
            switch (dir)
            {
            case UP:
                value = 0;
                break;
            case DOWN:
                value = 1;
                break;
            case LEFT:
                value = 2;
                break;
            case RIGHT:
                value = 1;
                break;
            }
            return std::format("E{}{}{}", row() + 1, col() + 1, value);
        }
        default:
            std::cerr << "display failed\n";
            exit(1);
        }
    }
};

struct piece
{
    int side;
    int8_t height;
};

struct zobrist
{
    uint64_t pst[12][64][2];
    uint64_t stage;
    uint64_t side2move;

    // Delete copy constructor and assignment operator to prevent duplicates
    zobrist(const zobrist&) = delete;
    zobrist& operator=(const zobrist&) = delete;

    // Static access method
    static zobrist& get()
    {
        static zobrist instance;
        return instance;
    }

private:
    explicit zobrist()
    {
        init_keys();
    }

    void init_keys()
    {
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

struct board
{
    std::array<int8_t, SIZE* SIZE> heights;
    std::array<uint64_t, SIDES> occ;

    int side2move;
    int moves;

    struct history
    {
        std::array<int8_t, SIZE* SIZE> heights;
        std::array<uint64_t, SIDES> occ;
        uint64_t hash;
    };

    int past_length;
    std::array<history, DRAW_LENGTH + 1> past;

    static board startpos()
    {
        board start = { .heights = {}, .occ = {}, .side2move = WHITE, .moves = 0, .past_length = 0, .past = {} };

        // create first history
        start.past[0].heights = start.heights;
        start.past[0].occ = start.occ;
        const zobrist& zob = zobrist::get();
        start.past[0].hash = zob.side2move;

        return start;
    }

    void make_move(move& m)
    {
        const zobrist& zob = zobrist::get();
        uint64_t hash = past[past_length].hash;
        hash ^= zob.side2move;
        if (moves + 1 == DROPS)
            hash ^= zob.stage;

        switch (m.type())
        {
        case move::NORMAL: {
            past[past_length].heights[m.square] = heights[m.square];
            past[past_length].heights[m.to()] = heights[m.to()];
            past[past_length].occ = occ;

            assert(heights[m.square] > 0);
            assert(occ[side2move] & (1ull << m.square));

            if (occ[side2move ^ 1] & (1ull << m.to()))
            {
                assert(heights[m.to()] > 0);
                // capture
                hash ^= zob.pst[heights[m.square]][m.to()][side2move];
                hash ^= zob.pst[heights[m.to()]][m.to()][side2move ^ 1];

                heights[m.to()] = heights[m.square];
                occ[side2move] |= (1ull << m.to());
                occ[side2move ^ 1] ^= (1ull << m.to());
            }
            else if (occ[side2move] & (1ull << m.to()))
            {
                assert(heights[m.to()] > 0);
                // stack
                hash ^= zob.pst[heights[m.to()]][m.to()][side2move];
                heights[m.to()] += heights[m.square];
                hash ^= zob.pst[heights[m.to()]][m.to()][side2move];
            }
            else
            {
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
            if (occ[side2move] & (1ull << m.square))
            {
                assert(heights[m.square] > 0);
                hash ^= zob.pst[heights[m.square]][m.square][side2move];
            }
            else
            {
                assert(heights[m.square] == 0);
                occ[side2move] |= (1ull << m.square);
            }

            heights[m.square] += PLACE_SIZE;
            hash ^= zob.pst[heights[m.square]][m.square][side2move];
            break;
        }
        case move::EXPAND: {
            // save
            past[past_length].heights = heights;
            past[past_length].occ = occ;

            std::array<int, SIZE> shifts{};

            int limit = m.edge_distance();
            int power = (int)heights[m.square] - 1;
            int before = power;
            for (int step = 1; step <= limit; ++step)
            {
                int sq = m.square + m.dir * step;
                assert(sq < 64 && sq >= 0);
                if ((occ[0] | occ[1]) & (1ull << sq))
                {
                    shifts[step] = before;
                }
                else
                {
                    shifts[step] = 0;

                    before -= 1;
                    if (before == 0)
                    {
                        limit = step;
                        break;
                    }
                }
            }

            for (int step = limit; step >= 1; --step)
            {
                int sq = m.square + m.dir * step;
                auto sq_data = at(sq);

                int shift = shifts[step];
                if (shift > 0 && sq_data.side != NONE)
                {
                    // try shift
                    if (step + shift <= limit)
                    {
                        int shift_sq = m.square + m.dir * (step + shift);

                        auto shift_sq_data = at(shift_sq);
                        if (shift_sq_data.side == NONE)
                        {
                            hash ^= zob.pst[sq_data.height][shift_sq][sq_data.side];

                            heights[shift_sq] = heights[sq];
                            occ[sq_data.side] |= (1ull << shift_sq);
                        }
                        else
                        {
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
                if (step <= power)
                {
                    assert(at(sq).side == NONE);

                    hash ^= zob.pst[1][sq][side2move];

                    heights[sq] = 1;
                    occ[side2move] |= (1ull << sq);
                }
            }

            hash ^= zob.pst[heights[m.square]][m.square][side2move];
            hash ^= zob.pst[1][m.square][side2move];

            heights[m.square] = 1;

            break;
        }
        default:
            std::cerr << "make_move failed\n";
            exit(1);
        }

        past_length += 1;
        past[past_length].hash = hash;
        side2move ^= 1;
        moves += 1;
    }

    void unmake_move(const move& m)
    {
        assert(past_length >= 1);

        past_length -= 1;
        moves -= 1;
        side2move ^= 1;

        switch (m.type())
        {
        case move::NORMAL: {
            heights[m.square] = past[past_length].heights[m.square];
            heights[m.to()] = past[past_length].heights[m.to()];
            occ = past[past_length].occ;
            break;
        }
        case move::PLACE: {
            heights[m.square] -= PLACE_SIZE;
            if (heights[m.square] == 0)
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

    uint64_t get_hash() const
    {
        return past[past_length].hash;
    }

    int get_state() const
    {
        if (moves < DROPS)
            return NONE;

        if (occ[side2move] == 0)
            return side2move ^ 1;

        if (moves >= DRAW_LENGTH)
            return DRAW;

        return NONE;
    }

    bool is_repetition(int ply) const
    {
        for (int i = 2; true; i += 2)
        {
            if (past_length - i < 0)
                break;

            if (past[past_length].hash == past[past_length - i].hash)
                return true;
        }

        return false;
    }

    piece at(int index) const
    {
        assert(index >= 0 && index < 64);
        if (occ[0] & (1ull << index))
            return { .side = 0, .height = heights[index] };

        if (occ[1] & (1ull << index))
            return { .side = 1, .height = heights[index] };

        return { .side = NONE, .height = 0 };
    }

    bool is_drop() const { return moves < DROPS; }

    bool is_legal(const move& m) const
    {
        switch (m.type())
        {
        case move::NORMAL: {
            auto start = at(m.square);
            auto end = at(m.to());
            if (start.side != side2move)
                return false;

            if (end.side != side2move && start.height <= end.height)
                return false;

            return true;
        }
        case move::PLACE: {
            auto start = at(m.square);
            if (start.side == side2move ^ 1)
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


    void display() const
    {
        for (int i = 0; i < SIZE * SIZE; ++i)
        {
            piece p = at(i);
            switch (p.side)
            {
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

struct movegen
{
    const board& m_board;

    std::vector<move> get_drops()
    {
        uint64_t occ = ~m_board.occ[m_board.side2move ^ 1];
        std::vector<move> moves{};
        while (occ)
        {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            moves.push_back(move::make_drop(idx));
        }

        return moves;
    }

    std::vector<move> get_normals()
    {
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ)
        {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            constexpr std::array<int, 4> dirs{ move::UP, move::DOWN, move::LEFT, move::RIGHT };
            for (int dir : dirs)
            {
                if (move::valid_normal(idx, dir))
                {
                    if (!(m_board.occ[m_board.side2move ^ 1] & (1ull << (idx + dir))) ||
                        m_board.heights[idx] > m_board.heights[idx + dir])
                        moves.push_back(move::make_normal(idx, m_board.heights[idx], dir));
                }
            }
        }
        return moves;
    }


    std::vector<move> get_expands()
    {
        std::vector<move> moves{};
        uint64_t occ = m_board.occ[m_board.side2move];
        while (occ)
        {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            if (m_board.heights[idx] == 1)
            {
                continue;
            }

            constexpr std::array<int, 4> dirs{ move::UP, move::DOWN, move::LEFT, move::RIGHT };
            for (int dir : dirs)
            {
                moves.push_back(move::make_expand(idx, dir));
            }
        }
        return moves;
    }

    std::vector<move> get_valids()
    {
        if (m_board.is_drop())
        {
            return get_drops();
        }

        auto expands = get_expands();
        auto normals = get_normals();
        expands.insert(expands.end(), std::make_move_iterator(normals.begin()), std::make_move_iterator(normals.end()));

        return expands;
    }

    std::vector<move> get_captures()
    {
        // expands
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ)
        {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            if (m_board.heights[idx] == 1)
            {
                continue;
            }

            constexpr std::array<int, 4> dirs{ move::UP, move::DOWN, move::LEFT, move::RIGHT };
            for (int dir : dirs)
            {
                moves.push_back(move::make_expand(idx, dir));
            }
        }

        // captures
        occ = m_board.occ[m_board.side2move];
        while (occ)
        {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            constexpr std::array<int, 4> dirs{ move::UP, move::DOWN, move::LEFT, move::RIGHT };
            for (int dir : dirs)
            {
                if (move::valid_normal(idx, dir))
                {
                    if ((m_board.occ[m_board.side2move ^ 1] & (1ull << (idx + dir))) &&
                        m_board.heights[idx] > m_board.heights[idx + dir])
                        moves.push_back(move::make_normal(idx, m_board.heights[idx], dir));
                }
            }
        }

        return moves;
    }

    std::vector<move> get_quiets()
    {
        // non-captures
        uint64_t occ = m_board.occ[m_board.side2move];
        std::vector<move> moves{};
        while (occ)
        {
            int idx = __builtin_ctzll(occ);
            occ ^= (1ull << idx);

            constexpr std::array<int, 4> dirs{ move::UP, move::DOWN, move::LEFT, move::RIGHT };
            for (int dir : dirs)
            {
                if (move::valid_normal(idx, dir))
                {
                    if (!(m_board.occ[m_board.side2move ^ 1] & (1ull << (idx + dir))))
                        moves.push_back(move::make_normal(idx, m_board.heights[idx], dir));
                }
            }
        }

        return moves;
    }
};
