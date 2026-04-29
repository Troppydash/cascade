#include <iostream>
#include "board.hpp"

int main() {
    board b = board::startpos();

    b.display();
    return 0;
}
