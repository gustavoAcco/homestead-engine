add_executable(test_serialization
    test_roundtrip.cpp
    test_schema_version.cpp
)

target_compile_features(test_serialization PRIVATE cxx_std_23)

target_link_libraries(test_serialization
    PRIVATE
        homestead::serialization
        homestead::solver
        homestead::graph
        homestead::core
        nlohmann_json::nlohmann_json
        Catch2::Catch2WithMain
)

catch_discover_tests(test_serialization)
