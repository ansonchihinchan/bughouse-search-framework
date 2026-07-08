add_library(search
    alpha_beta_search.cpp
    null_move_search.cpp
    pvs.cpp
    transposition_table.cpp
    tree_search.cpp
)

target_include_directories(search 
    PUBLIC 
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(search 
    PUBLIC game
)