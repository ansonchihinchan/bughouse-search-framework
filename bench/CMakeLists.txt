add_executable(bench_search
    bench.cpp
    positions.cpp
)

target_link_libraries(bench_search
    PRIVATE
        game
        search
)

target_include_directories(bench_search
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)
