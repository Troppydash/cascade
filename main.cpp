#include <iostream>
#include <string>
#include "board.hpp"
#include "engine.hpp"

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
                std::cout << "\n";
            } else if (command == "newgame") {
                tt.reset();
            } else if (command == "go") {
                int64_t time = std::stoll(parts[1]);

                std::vector<move> moves{};
                for (int i = 2; i < parts.size(); ++i)
                    moves.push_back(move::of_string(parts[i]));

                auto board = movegen::create_board(moves);
                engine eng{board, &tt};
                auto result = eng.search(1000, 2000);
                std::cout << "bestmove " << result.m.str() << std::endl;
            } else {
                std::cout << "Unknown command '" << command << "'\n";
            }
        }
    }
};

int main() {
    uci uci;
    uci.loop();
    return 0;
}
