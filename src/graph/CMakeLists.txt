add_library(homestead_graph STATIC
    graph.cpp
)

add_library(homestead::graph ALIAS homestead_graph)

target_compile_features(homestead_graph PUBLIC cxx_std_23)

target_include_directories(homestead_graph
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(homestead_graph
    PUBLIC homestead::core
)
