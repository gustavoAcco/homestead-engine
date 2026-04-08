add_executable(test_core
    test_quantity.cpp
    test_resource.cpp
    test_entity.cpp
    test_registry.cpp
)

target_compile_features(test_core PRIVATE cxx_std_23)

target_link_libraries(test_core
    PRIVATE
        homestead::core
        Catch2::Catch2WithMain
)

catch_discover_tests(test_core)
