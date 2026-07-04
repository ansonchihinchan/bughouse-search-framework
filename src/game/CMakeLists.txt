add_library(game
    board.cpp
    bughouse.cpp
    clock.cpp
    movegen.cpp
    pocket.cpp
)

target_include_directories(game
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(bughouse
    main.cpp
)

target_link_libraries(bughouse
    PRIVATE game
)