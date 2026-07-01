cmake_minimum_required(VERSION 3.20)

project(Bughouse
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(bughouse
    src/game/main.cpp
    src/game/board.cpp
    src/game/bughouse.cpp
    src/game/clock.cpp
    src/game/movegen.cpp
    src/game/pocket.cpp
)

target_include_directories(bughouse
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# Optional compiler warnings
if(MSVC)
    target_compile_options(bughouse PRIVATE /W4)
else()
    target_compile_options(bughouse PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()