cmake_minimum_required(VERSION 3.20)

project(Bughouse
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_subdirectory(src/game)
add_subdirectory(src/search)
add_subdirectory(src/eval)
add_subdirectory(test/game)
add_subdirectory(test/eval)
add_subdirectory(bench)