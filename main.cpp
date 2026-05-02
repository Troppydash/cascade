#include <atomic>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include "board.hpp"
#include "engine.hpp"
#include "pstream.h"

inline std::vector<std::string> string_split(std::string const &input) {
    std::stringstream ss(input);

    std::vector<std::string> words((std::istream_iterator<std::string>(ss)), std::istream_iterator<std::string>());

    return words;
}

struct uci {

    explicit uci() {}

    void loop() {
        tt tt{256};

        while (true) {
            std::string line;
            std::getline(std::cin, line);

            auto parts = string_split(line);
            std::string command = parts[0];
            if (command == "uci") {
                std::cout << "Cascade solver by troppydash\n";
                std::cout << "version 1.0.0\n";
                std::cout << "ok\n";
            } else if (command == "newgame") {
                tt.reset();
                std::cout << "ok\n";
            } else if (command == "go") {
                int64_t time = std::stoll(parts[1]);

                std::vector<move> moves{};
                for (int i = 2; i < parts.size(); ++i)
                    moves.push_back(move::of_string(parts[i]));

                auto board = movegen::create_board(moves);
                engine eng{board, &tt};
                auto result = eng.search(100, 200);
                std::cout << "bestmove " << result.m.str() << std::endl;
            } else {
                std::cout << "Unknown command '" << command << "'\n";
            }
        }
    }
};

struct uci_wrapper {
    redi::pstream stream;
    explicit uci_wrapper(const std::string &path) : stream{path} {}

    void newgame() {
        stream << "newgame\n";
        stream.flush();

        std::string out;
        std::getline(stream.out(), out);
        if (out != "ok") {
            std::cerr << "invalid newgame response " << out << std::endl;
            exit(0);
        }
    }

    move search(int64_t time, const std::vector<move> &moves) {
        stream << "go " << time;
        for (auto &m: moves) {
            stream << " " << m.str();
        }
        stream << "\n";
        stream.flush();

        std::string out;
        while (true) {
            std::getline(stream.out(), out);
            // std::cout << "info:" << out << std::endl;
            if (out.starts_with("bestmove")) {
                auto parts = string_split(out);
                return move::of_string(parts[1]);
            }
        }
    }
};

struct result {
    // [LL, ..., WW]
    std::array<int, 5> pent{};

    void add(std::pair<float, float> result) {
        // 1 for win, 0 for lose
        pent[(int) std::round(result.first + result.second + 2)] += 1;
    }

    void display() {
        std::cout << "[ ";
        for (auto &i: pent)
            std::cout << i << " ,";

        std::cout << "]\n";
    }
};

struct runner {
    uci_wrapper alpha;
    uci_wrapper beta;

    std::mt19937_64 gen;

    explicit runner(const std::string &alpha, const std::string &beta) : alpha{alpha}, beta{beta} {
        std::random_device dev;
        gen = std::mt19937_64(dev());
    }

    std::vector<move> get_opening(int depth) {
        board board{};
        std::vector<move> moves{};

        for (int i = 0; i < depth && board.get_state() == NONE; ++i) {
            auto all = movegen{board}.get_valids();
            std::uniform_int_distribution<int> dist{0, (int) all.size() - 1};
            move m = all[dist(gen)];
            moves.push_back(m);
            board.make_move(m);
        }

        return moves;
    }

    std::pair<float, float> round(std::atomic<int> &num) {
        int depth = 7;
        auto opening = get_opening(depth);

        auto game = [this, &opening, &num](int alpha_side2move) -> double {
            int n = num.fetch_add(1);
            std::cout << "starting game: " << n;
            if (alpha_side2move == WHITE)
                std::cout << " ALPHA vs BETA\n";
            else
                std::cout << " BETA vs ALPHA\n";

            std::array<int64_t, 2> clock{180 * 1000, 180 * 1000};

            std::vector<move> moves{opening.begin(), opening.end()};
            board board = movegen::create_board(moves);

            this->alpha.newgame();
            this->beta.newgame();

            while (board.get_state() == NONE) {
                move m;
                auto start = timer::now();
                if (board.side2move == alpha_side2move) {
                    m = this->alpha.search(clock[board.side2move], moves);
                } else {
                    m = this->beta.search(clock[board.side2move], moves);
                }
                clock[board.side2move] -= timer::now() - start;
                if (clock[board.side2move] < 0)
                    return board.side2move ^ 1;

                board.make_move(m);
                moves.push_back(m);
            }

            double result = 0.0;
            if (board.get_state() == DRAW) {
                std::cout << "game " << n << " result: DRAW\n";
                result = 0;
            } else if (board.get_state() == alpha_side2move) {
                std::cout << "game " << n << " result: ALPHA WINS\n";
                result = 1.0;
            } else {
                std::cout << "game " << n << " result: BETA WINS\n";
                result = -1.0;
            }

            return result;
        };

        return {game(0), game(1)};
    }
};

int main(int argc, char **argv) {
    // check for runner
    if (argc >= 4 && std::string{argv[1]} == "runner") {
        std::atomic<int> num = 0;
        runner run{std::string{argv[2]}, std::string{argv[3]}};
        result res{};
        while (true) {
            res.display();
            auto score = run.round(num);
            res.add(score);
        }

        return 0;
    }

    uci uci;
    uci.loop();
    return 0;
}
