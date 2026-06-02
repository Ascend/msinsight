project(sqlite)

set(CMAKE_CXX_STANDARD 17)

include_directories(./include)
aux_source_directory(./src SRC)

set(CMAKE_C_VISIBILITY_PRESET default)
set(LIBRARY_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH})
add_library(${PROJECT_NAME} SHARED ${SRC})
