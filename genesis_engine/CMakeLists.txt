cmake_minimum_required(VERSION 3.20)
project(GenesisEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(genesis_sim
  sim_core/src/main.cpp
  sim_core/src/ipc/SharedMemoryBridge.cpp
)

add_executable(shm_reader
  sim_core/src/tools/shm_reader.cpp
  sim_core/src/ipc/SharedMemoryBridge.cpp
)

target_include_directories(genesis_sim
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
)

target_include_directories(shm_reader
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/sim_core/include
)

if (MSVC)
  target_compile_definitions(genesis_sim PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_definitions(shm_reader PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
