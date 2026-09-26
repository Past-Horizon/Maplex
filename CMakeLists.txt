cmake_minimum_required (VERSION 3.10)

project ("Maplex")

include(cmake/MaplexSource.cmake)

add_executable (Maplex ${MAPLEX_SOURCES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Maplex PROPERTY CXX_STANDARD 20)
endif()

