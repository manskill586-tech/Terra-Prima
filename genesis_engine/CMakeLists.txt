cmake_minimum_required(VERSION 3.20)
project(GenesisEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)

FetchContent_Declare(
  yaml_cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.7.0
)

FetchContent_MakeAvailable(nlohmann_json yaml_cpp)

find_package(Threads REQUIRED)

add_library(terra_chem STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/chem/ChemDB.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/chem/GeoCycles.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/chem/PhaseModel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/chem/ReactionEngine.cpp
)

target_include_directories(terra_chem
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(terra_chem
  PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
    yaml-cpp
)

add_executable(genesis_sim
  sim_core/src/main.cpp
  sim_core/src/ipc/SharedMemoryBridge.cpp
  sim_core/src/world/ChunkStore.cpp
  sim_core/src/sim/SimScheduler.cpp
  sim_core/src/sim/WorldSim.cpp
  sim_core/src/sim/ParticleSystem.cpp
  sim_core/src/persistence/Snapshot.cpp
)

add_executable(shm_reader
  sim_core/src/tools/shm_reader.cpp
  sim_core/src/ipc/SharedMemoryBridge.cpp
)

target_include_directories(genesis_sim
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_include_directories(shm_reader
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
)

if (MSVC)
  target_compile_definitions(genesis_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(shm_reader PRIVATE _CRT_SECURE_NO_WARNINGS)
  # Large SHM structs can trigger C1060 in MSVC; give the compiler more heap.
  target_compile_options(genesis_sim PRIVATE /Zm200)
  target_compile_options(shm_reader PRIVATE /Zm200)
endif()

target_link_libraries(genesis_sim PRIVATE terra_chem)
