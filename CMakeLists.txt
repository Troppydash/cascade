cmake_minimum_required(VERSION 4.0)
project(cascade)

set(CMAKE_CXX_STANDARD 20)

add_executable(cascade main.cpp)
target_compile_options(cascade PRIVATE
        "-O3"
        "-march=native"
        "-mtune=native"
        "-fomit-frame-pointer"
        "-Wall"
        "-Wextra"
        -DNDEBUG
        -mcpu=native
        -fno-exceptions
)
