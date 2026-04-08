cmake_minimum_required(VERSION 3.20)
project(TerraPrima LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(terra_core
  src/Clock.cpp
  src/JobSystem.cpp
  src/SimScheduler.cpp
  src/ChunkStore.cpp
  src/EntityLOD.cpp
  src/SnapshotStore.cpp
  src/World.cpp
  src/Renderer.cpp
)

target_include_directories(terra_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(terra_core PUBLIC Threads::Threads)

target_compile_definitions(terra_core PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>)

add_executable(terra_demo src/main.cpp)
target_link_libraries(terra_demo PRIVATE terra_core)

add_executable(terra_tests
  tests/TestMain.cpp
  tests/SimSchedulerTests.cpp
  tests/ChunkStoreTests.cpp
)

target_link_libraries(terra_tests PRIVATE terra_core)

include(CTest)
add_test(NAME terra_tests COMMAND terra_tests)
