add_executable(homestead_cli
    main.cpp
)

target_compile_features(homestead_cli PRIVATE cxx_std_23)

target_link_libraries(homestead_cli
    PRIVATE
        homestead::core
        homestead::graph
        homestead::solver
        homestead::serialization
        nlohmann_json::nlohmann_json
)
