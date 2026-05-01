#include <iostream>
#include "board.hpp"
#include "engine.hpp"

void loop() {
    board b = board::startpos();
    srand(42);
    while (true) {
        b.display();
        if (b.get_state() != NONE) {
            break;
        }

        movegen gen{b};
        auto moves = gen.get_valids();
        std::cout << moves.size() << std::endl;
        int rng = rand() % (moves.size());
        b.make_move(moves[rng]);
        moves[rng].display();
        std::cout << '\n';
    }
}

void search() {
    board b = board::startpos();

    engine eng{b};
    int64_t time = 2 * 1000;
    eng.search(time, time);
}

void selfplay() {
    board b = board::startpos();

    while (b.get_state() == NONE) {
        b.display();

        engine eng{b};
        int64_t time = 2 * 1000;
        auto result = eng.search(time, time);

        b.make_move(result.m);
    }
}

int main() {
    selfplay();
    return 0;
}
