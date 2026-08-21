add_library(game
    attacks.cpp
    board.cpp
    bughouse.cpp
    clock.cpp
    movegen.cpp
    pocket.cpp
    zobrist.cpp
)

target_include_directories(game
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)