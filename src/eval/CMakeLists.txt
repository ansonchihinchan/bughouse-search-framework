add_library(eval
    classical.cpp
    score.cpp
    types.cpp
    classical/activity.cpp
    classical/king_safety.cpp
    classical/material.cpp
    classical/mobility.cpp
    classical/pawn.cpp
    classical/piece_square.cpp
    classical/space.cpp
    classical/tempo.cpp
)

target_include_directories(eval
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(eval
    PUBLIC game
)