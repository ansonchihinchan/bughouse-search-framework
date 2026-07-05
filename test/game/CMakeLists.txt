find_package(Catch2 3 REQUIRED)

include(Catch)

add_executable(game_tests
    test_board.cpp
    test_bughouse.cpp
    test_clock.cpp
    test_movegen.cpp
    test_pocket.cpp
    test_types.cpp
)

target_link_libraries(game_tests
    PRIVATE
        game
        Catch2::Catch2WithMain
)

target_include_directories(game_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

catch_discover_tests(game_tests)