add_library(homestead_serialization STATIC
    serialization.cpp
)

add_library(homestead::serialization ALIAS homestead_serialization)

target_compile_features(homestead_serialization PUBLIC cxx_std_23)

target_include_directories(homestead_serialization
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(homestead_serialization
    PUBLIC  homestead::core
    PRIVATE nlohmann_json::nlohmann_json
)
