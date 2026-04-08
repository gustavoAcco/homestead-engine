add_executable(test_graph
    test_graph_construction.cpp
    test_cycle_detection.cpp
    test_traversal.cpp
)

target_compile_features(test_graph PRIVATE cxx_std_23)

target_link_libraries(test_graph
    PRIVATE
        homestead::graph
        Catch2::Catch2WithMain
)

catch_discover_tests(test_graph)
