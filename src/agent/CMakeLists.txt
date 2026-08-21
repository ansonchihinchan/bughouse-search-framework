add_library(agent
    experiment.cpp
    replay.cpp
    round_robin.cpp
    self_play.cpp
    observer.cpp
    temporal.cpp
    tournament.cpp
    types.cpp
)

target_include_directories(agent
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(agent
    PUBLIC
        communication
        eval
        game
        search
)