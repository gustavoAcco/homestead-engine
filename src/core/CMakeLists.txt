add_library(homestead_core STATIC
    quantity.cpp
    resource.cpp
    registry.cpp
    default_registry_data.cpp
)

add_library(homestead::core ALIAS homestead_core)

target_compile_features(homestead_core PUBLIC cxx_std_23)

target_include_directories(homestead_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# homestead::core MUST NOT link any non-stdlib dependency.
# Logging is provided by the private facade in src/core/log.hpp only.
if(HOMESTEAD_ENABLE_LOGGING)
    target_link_libraries(homestead_core PRIVATE spdlog::spdlog)
    target_compile_definitions(homestead_core PRIVATE HOMESTEAD_ENABLE_LOGGING)
endif()
