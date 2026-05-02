cmake_minimum_required(VERSION 4.0)
project(cascade)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_executable(cascade main.cpp)
target_compile_options(cascade PRIVATE
        "-O3"
        "-ffast-math"
        "-march=native"
        "-mtune=native"
        "-fomit-frame-pointer"
        "-Wall"
        "-Wextra"
        -DNDEBUG
        -fno-exceptions
)
