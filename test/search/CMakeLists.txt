find_package(Catch2 3 REQUIRED)

include(Catch)

add_executable(search_tests
    test_history.cpp
    test_killer.cpp
    test_see.cpp
    test_transposition_table.cpp
    test_tree_search.cpp
)

target_link_libraries(search_tests
    PRIVATE
        eval
        search
        game
        Catch2::Catch2WithMain
)

target_include_directories(search_tests
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

catch_discover_tests(search_tests)