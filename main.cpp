#include <atomic>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "board.hpp"
#include "engine.hpp"
#include "pstream.h"

// #define DEBUG

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
#ifdef DEBUG
            std::cout << "info: " << out << std::endl;
#endif
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
        std::cout << "======================\n";
        std::cout << "pent: [";
        for (auto &i: pent)
            std::cout << " " << i;

        std::cout << "]\n";
        stats();
        std::cout << "======================\n";
    }

    void stats() {
        // Thanks LLM AI
        double N = std::accumulate(pent.begin(), pent.end(), 0.0);

        if (N <= 0)
            return;

        // Calculate Probabilities (P_i)
        std::vector<double> p(5);
        for (int i = 0; i < 5; ++i) {
            p[i] = pent[i] / N;
        }

        // Mean (mu) and Variance (sigma^2)
        double mu = 0.0;
        for (int i = 0; i < 5; ++i)
            mu += i * p[i];

        double var = 0.0;
        for (int i = 0; i < 5; ++i)
            var += std::pow(i, 2) * p[i];
        var -= std::pow(mu, 2);

        // Standard Error of the mean
        double st_error = std::sqrt(std::max(0.0, var)) / std::sqrt(N);

        // Mean score per game (s) on scale [0, 1]
        // mu is for a pair (max 4 pts), so we divide by 4
        double s = mu / 4.0;

        // Clamp s to avoid log(0) or division by zero
        s = std::max(1e-6, std::min(1.0 - 1e-6, s));

        // Calculate Elo Gain
        double elo_diff = -400.0 * std::log10(1.0 / s - 1.0);

        // Elo Error Margin calculation via the Delta Method
        // Derivative of the Elo formula: 400 / (s * (1 - s) * ln(10))
        const double confidence_z = 1.96;
        double phi = st_error * confidence_z;
        double elo_derivative = 400.0 / (s * (1.0 - s) * std::log(10.0));
        double elo_error = elo_derivative * (phi / 4.0);

        std::cout << "elo: " << elo_diff << " +- " << elo_error << "\n";
        std::cout << "points " << s * 100.0 << "%\n";
    }
};

std::mutex runner_lock;

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
        int depth = 3;
        auto opening = get_opening(depth);

        auto game = [this, &opening, &num](int alpha_side2move) -> double {
            int n = num.fetch_add(1);
            runner_lock.lock();
            std::cout << "starting game " << n << ":";
            if (alpha_side2move == WHITE)
                std::cout << " ALPHA vs BETA\n";
            else
                std::cout << " BETA vs ALPHA\n";
            runner_lock.unlock();

            std::array<int64_t, 2> clock{180 * 1000, 180 * 1000};

            std::vector<move> moves{opening.begin(), opening.end()};
            board board = movegen::create_board(moves);

            this->alpha.newgame();
            this->beta.newgame();

            while (board.get_state() == NONE) {
#ifdef DEBUG
                board.display();
#endif
                move m;
                auto start = timer::now();
                if (board.side2move == alpha_side2move) {
                    m = this->alpha.search(clock[board.side2move], moves);
                } else {
                    m = this->beta.search(clock[board.side2move], moves);
                }
                clock[board.side2move] -= timer::now() - start;
                if (clock[board.side2move] < 0) {
                    runner_lock.lock();
                    std::cout << "game " << n << " result: OUT OF TIME\n";
                    runner_lock.unlock();

                    return board.side2move ^ 1;
                }

                board.make_move(m);
                moves.push_back(m);
            }

            runner_lock.lock();
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
            runner_lock.unlock();


            return result;
        };

        return {game(0), game(1)};
    }
};

int main(int argc, char **argv) {
    // check for runner
    if (argc >= 4 && std::string{argv[1]} == "runner") {
        std::string alpha = std::string{argv[2]};
        std::string beta = std::string{argv[3]};
        std::atomic<int> num = 0;

        int thread_count = 14;
        std::vector<std::thread> threads;
        std::mutex res_lock;
        result res{};

        for (int i = 0; i < thread_count; ++i) {
            threads.push_back(std::thread([alpha, beta, &num, &res, &res_lock]() {
                runner run{alpha, beta};
                while (true) {
                    auto score = run.round(num);
                    res_lock.lock();
                    res.add(score);
                    res_lock.unlock();

                    if (num.load() % 2 == 0) {
                        res_lock.lock();
                        res.display();
                        res_lock.unlock();
                    }
                }
            }));
        }

        for (int i = 0; i < thread_count; ++i)
            threads[i].join();

        return 0;
    }

    uci uci;
    uci.loop();
    return 0;
}
