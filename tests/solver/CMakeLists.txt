add_executable(test_solver
    test_backpropagation.cpp
    test_convergence.cpp
    test_scheduling.cpp
    test_constraints.cpp
    test_analytics.cpp
    test_properties.cpp
    test_quickstart.cpp
)

target_compile_features(test_solver PRIVATE cxx_std_23)

target_link_libraries(test_solver
    PRIVATE
        homestead::solver
        homestead::core
        homestead::graph
        Catch2::Catch2WithMain
)

catch_discover_tests(test_solver)

# Benchmark — separate executable so timing failures don't block the test suite
add_executable(bench_solver
    bench_solver.cpp
)

target_compile_features(bench_solver PRIVATE cxx_std_23)

target_link_libraries(bench_solver
    PRIVATE
        homestead::solver
        homestead::core
        homestead::graph
        Catch2::Catch2WithMain
)

catch_discover_tests(bench_solver TEST_PREFIX "bench_")
