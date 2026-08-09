add_library(communication
    context.cpp
)

target_include_directories(communication
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(communication
    PUBLIC game
)