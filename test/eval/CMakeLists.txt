find_package(Catch2 3 REQUIRED)

include(Catch)

add_executable(eval_tests
    test_classical.cpp
    test_score.cpp
    test_types.cpp
    bughouse/test_drop.cpp
    bughouse/test_exchange.cpp
    bughouse/test_initiative.cpp
    bughouse/test_king_danger.cpp
    bughouse/test_partner.cpp
    bughouse/test_pocket.cpp    
    bughouse/test_prediction.cpp
    classical/test_activity.cpp    
    classical/test_king_safety.cpp
    classical/test_material.cpp
    classical/test_mobility.cpp
    classical/test_pawn.cpp
    classical/test_piece_square.cpp
    classical/test_space.cpp
    classical/test_tempo.cpp    
)

target_link_libraries(eval_tests
    PRIVATE
        eval
        game
        Catch2::Catch2WithMain
)

target_include_directories(eval_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

catch_discover_tests(eval_tests)