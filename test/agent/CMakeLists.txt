find_package(Catch2 3 REQUIRED)

include(Catch)

add_executable(agent_tests
    test_self_play.cpp
    test_types.cpp
)

target_link_libraries(agent_tests
    PRIVATE
        agent
        Catch2::Catch2WithMain
)

catch_discover_tests(agent_tests)