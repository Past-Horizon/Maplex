cmake_minimum_required(VERSION 3.20)

project(Maplex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

if(WIN32)
  set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING
    "vcpkg triplet used for Windows dependencies")
elseif(UNIX)
  set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING
    "vcpkg triplet used for Linux dependencies")
endif()

set(MAPLEX_VCPKG_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
if(NOT EXISTS "${MAPLEX_VCPKG_PREFIX}/share/nlohmann_json/nlohmann_jsonConfig.cmake")
  message(FATAL_ERROR
    "Could not locate nlohmann-json for triplet '${VCPKG_TARGET_TRIPLET}' "
    "under ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed. Install the manifest "
    "dependencies with this triplet before configuring Maplex.")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${MAPLEX_VCPKG_PREFIX}")
find_package(nlohmann_json CONFIG REQUIRED)

if (NOT TARGET nlohmann_json::nlohmann_json)
  add_library(MaplexJson INTERFACE)
  target_include_directories(MaplexJson SYSTEM INTERFACE "${nlohmann_json_SOURCE_DIR}/include")
  add_library(nlohmann_json::nlohmann_json ALIAS MaplexJson)
endif()

include(cmake/MaplexSource.cmake)

set(MAPLEX_APP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/Main.cpp")

add_library(MaplexCore ${MAPLEX_SOURCES})
add_library(Maplex::Core ALIAS MaplexCore)
target_include_directories(MaplexCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(MaplexCore PRIVATE nlohmann_json::nlohmann_json)

add_executable(Maplex "${MAPLEX_APP_SOURCE}")
target_link_libraries(Maplex PRIVATE Maplex::Core)

include(CTest)

if (BUILD_TESTING)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  endif()

  FetchContent_MakeAvailable(googletest)

  add_executable(MaplexTests
    tests/MappingTests.cpp
    tests/CipherTests.cpp
  )

  target_link_libraries(MaplexTests
    PRIVATE
      Maplex::Core
      GTest::gtest_main
  )

  include(GoogleTest)
  gtest_discover_tests(MaplexTests)
endif()

