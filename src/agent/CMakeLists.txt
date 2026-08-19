add_library(agent
    experiment.cpp
    self_play.cpp
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