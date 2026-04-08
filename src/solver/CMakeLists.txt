add_library(homestead_solver STATIC
    result.cpp
    backpropagation.cpp
    convergence.cpp
    scheduling.cpp
    analytics.cpp
)

add_library(homestead::solver ALIAS homestead_solver)

target_compile_features(homestead_solver PUBLIC cxx_std_23)

target_include_directories(homestead_solver
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(homestead_solver
    PUBLIC homestead::core homestead::graph
)
