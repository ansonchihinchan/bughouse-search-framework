add_library(search
    tree_search.cpp
    alpha_beta_search.cpp
)

target_include_directories(search PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(search PUBLIC game)