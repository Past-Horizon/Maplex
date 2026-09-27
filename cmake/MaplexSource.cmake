file(GLOB_RECURSE MAPLEX_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/*.h"
  "${CMAKE_CURRENT_LIST_DIR}/../include/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../include/*.h"
)

list(FILTER MAPLEX_SOURCES EXCLUDE REGEX "[/\\\\]src[/\\\\]Main\\.cpp$")