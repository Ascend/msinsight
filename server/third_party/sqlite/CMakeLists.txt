project(sqlite)

set(CMAKE_CXX_STANDARD 17)

include_directories(./include)
aux_source_directory(./src SRC)

set(CMAKE_C_VISIBILITY_PRESET default)
set(LIBRARY_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH})
add_library(${PROJECT_NAME} SHARED ${SRC})
if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_link_options(${PROJECT_NAME} PRIVATE
        "-Wl,--exclude-symbols=__stack_chk_fail"
        "-Wl,--exclude-symbols=__stack_chk_fail_local"
        "-Wl,--exclude-symbols=__stack_chk_guard")
endif()
