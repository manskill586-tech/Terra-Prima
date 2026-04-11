cmake_minimum_required(VERSION 3.20)
project(TerraPrima LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

find_package(Threads REQUIRED)
include(FetchContent)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG 0.8.0
)

FetchContent_MakeAvailable(nlohmann_json yaml-cpp)

add_library(terra_core
  src/Clock.cpp
  src/JobSystem.cpp
  src/SimScheduler.cpp
  src/ChunkStore.cpp
  src/EntityLOD.cpp
  src/SnapshotStore.cpp
  src/chem/ChemDB.cpp
  src/chem/ReactionEngine.cpp
  src/chem/PhaseModel.cpp
  src/chem/GeoCycles.cpp
  src/World.cpp
  src/Renderer.cpp
)

target_include_directories(terra_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(terra_core PUBLIC Threads::Threads nlohmann_json::nlohmann_json yaml-cpp)

target_compile_definitions(terra_core PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

add_executable(terra_demo src/main.cpp)
target_link_libraries(terra_demo PRIVATE terra_core)

add_executable(chemdb_tool src/tools/chemdb_tool.cpp)
target_link_libraries(chemdb_tool PRIVATE terra_core)

add_executable(terra_tests
  tests/TestMain.cpp
  tests/SimSchedulerTests.cpp
  tests/ChunkStoreTests.cpp
  tests/SnapshotTests.cpp
)

target_link_libraries(terra_tests PRIVATE terra_core)

include(CTest)
add_test(NAME terra_tests COMMAND terra_tests)
