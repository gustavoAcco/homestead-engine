cmake_minimum_required(VERSION 3.28)

project(homestead-engine
    VERSION 0.1.0
    DESCRIPTION "Integrated food production planning engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Options ────────────────────────────────────────────────────────────────────
option(HOMESTEAD_BUILD_TESTS    "Build Catch2 test suite"             ON)
option(HOMESTEAD_BUILD_CLI      "Build the minimal CLI harness"       OFF)
option(HOMESTEAD_ENABLE_LOGGING "Enable spdlog-based solver logging"  OFF)

# ── Dependencies via FetchContent ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE)

if(HOMESTEAD_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.6.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)
    include(CTest)
    include(Catch)
endif()

if(HOMESTEAD_ENABLE_LOGGING)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.13.0
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(spdlog)
endif()

FetchContent_MakeAvailable(nlohmann_json)

# ── Library targets ────────────────────────────────────────────────────────────
add_subdirectory(src/core)
add_subdirectory(src/graph)
add_subdirectory(src/solver)
add_subdirectory(src/serialization)

if(HOMESTEAD_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

# ── Tests ──────────────────────────────────────────────────────────────────────
if(HOMESTEAD_BUILD_TESTS)
    add_subdirectory(tests/core)
    add_subdirectory(tests/graph)
    add_subdirectory(tests/solver)
    add_subdirectory(tests/serialization)
endif()
