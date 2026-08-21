find_package(Catch2 3 REQUIRED)

include(Catch)

add_executable(communication_tests
    test_generator.cpp
)

target_link_libraries(communication_tests
    PRIVATE
        communication
        Catch2::Catch2WithMain
)

catch_discover_tests(communication_tests)